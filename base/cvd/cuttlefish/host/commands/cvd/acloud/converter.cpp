/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cuttlefish/host/commands/cvd/acloud/converter.h"

#include <sys/stat.h>

#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <google/protobuf/text_format.h>

#include "cuttlefish/common/libs/fs/shared_fd.h"
#include "cuttlefish/common/libs/utils/environment.h"
#include "cuttlefish/common/libs/utils/files.h"
#include "cuttlefish/common/libs/utils/flag_parser.h"
#include "cuttlefish/common/libs/utils/result.h"
#include "cuttlefish/common/libs/utils/subprocess.h"
#include "cuttlefish/common/libs/utils/subprocess_managed_stdio.h"
#include "cuttlefish/host/commands/cvd/acloud/config.h"
#include "cuttlefish/host/commands/cvd/acloud/create_converter_parser.h"
#include "cuttlefish/host/commands/cvd/cli/command_request.h"
#include "cuttlefish/host/commands/cvd/cli/selector/selector_constants.h"
#include "cuttlefish/host/commands/cvd/instances/device_name.h"
#include "cuttlefish/host/commands/cvd/instances/lock/lock_file.h"
#include "cuttlefish/host/commands/cvd/utils/common.h"
#include "cuttlefish/host/libs/config/config_constants.h"

namespace cuttlefish {
namespace {

// Image names to search
const std::vector<std::string> kKernelImageNames = {"kernel", "bzImage",
                                                    "Image"};
const std::vector<std::string> kInitRamFsImageName = {"initramfs.img"};
const std::vector<std::string> kBootImageName = {"boot.img"};
const std::vector<std::string> kVendorBootImageName = {"vendor_boot.img"};
const std::string kMixedSuperImageName = "mixed_super.img";

struct BranchBuildTargetInfo {
  std::string branch_str;
  std::string build_target_str;
};

static Result<BranchBuildTargetInfo> GetDefaultBranchBuildTarget(
    const std::string default_branch_str) {
  // get the default build branch and target from repo info and git remote
  BranchBuildTargetInfo result_info;
  result_info.branch_str = default_branch_str;
  Command repo_cmd("repo");
  repo_cmd.AddParameter("info");
  repo_cmd.AddParameter("platform/tools/acloud");

  auto cuttlefish_source =
      StringFromEnv("ANDROID_BUILD_TOP", "") + "/tools/acloud";
  auto fd_top =
      SharedFD::Open(cuttlefish_source, O_RDONLY | O_PATH | O_DIRECTORY);
  if (!fd_top->IsOpen()) {
    LOG(ERROR) << "Couldn't open \"" << cuttlefish_source
               << "\": " << fd_top->StrError();
  } else {
    repo_cmd.SetWorkingDirectory(fd_top);
  }

  std::string repo_stdout = CF_EXPECT(RunAndCaptureStdout(std::move(repo_cmd)));

  Command git_cmd("git");
  git_cmd.AddParameter("remote");
  if (fd_top->IsOpen()) {
    git_cmd.SetWorkingDirectory(fd_top);
  }

  std::string git_stdout = CF_EXPECT(RunAndCaptureStdout(std::move(git_cmd)));

  git_stdout.erase(std::remove(git_stdout.begin(), git_stdout.end(), '\n'),
                   git_stdout.cend());

  static const std::regex repo_rgx("^Manifest branch: (.+)");
  std::smatch repo_matched;
  CHECK(std::regex_search(repo_stdout, repo_matched, repo_rgx))
      << "Manifest branch line is not found from: " << repo_stdout;
  // master or ...
  std::string repo_matched_str = repo_matched[1].str();
  if (git_stdout == "aosp") {
    result_info.branch_str = "aosp-";
    result_info.build_target_str = "aosp_";
  }
  result_info.branch_str += repo_matched_str;

  // AVD_TYPES_MAPPING default is cf
  // _DEFAULT_BUILD_BITNESS default is x86_64
  // flavor default is phone
  // _DEFAULT_BUILD_TYPE default is userdebug
  result_info.build_target_str += "cf_x86_64_phone-userdebug";
  return result_info;
}

/**
 * Split a string into arguments based on shell tokenization rules.
 *
 * This behaves like `shlex.split` from python where arguments are separated
 * based on whitespace, but quoting and quote escaping is respected. This
 * function effectively removes one level of quoting from its inputs while
 * making the split.
 */
Result<std::vector<std::string>> BashTokenize(const std::string& str) {
  Command command("bash");
  command.AddParameter("-c");
  command.AddParameter("printf '%s\n' ", str);

  std::string bash_stdout = CF_EXPECT(RunAndCaptureStdout(std::move(command)));

  return android::base::Split(bash_stdout, "\n");
}

}  // namespace

namespace acloud_impl {

Result<ConvertedAcloudCreateCommand> ConvertAcloudCreate(
    const CommandRequest& request) {
  std::vector<std::string> arguments = request.SubcommandArguments();
  CF_EXPECT(!arguments.empty());
  CF_EXPECT(arguments[0] == "create");
  arguments.erase(arguments.begin());

  /*
   * TODO(chadreynolds@): Move all the flag parsing eventually to the
   * converter_parser.{h,cpp}.
   *
   * Note that the transfer should be done from the top through the bottom.
   * ConsumeFlags() parses each flag in order.
   */
  auto parsed_flags = CF_EXPECT(acloud_impl::ParseAcloudCreateFlags(arguments));

  std::vector<Flag> flags;

  std::optional<std::string> boot_build_id;
  flags.emplace_back(
      Flag()
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--boot-build-id"})
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--boot_build_id"})
          .Setter([&boot_build_id](const FlagMatch& m) -> Result<void> {
            boot_build_id = m.value;
            return {};
          }));
  std::optional<std::string> boot_build_target;
  flags.emplace_back(
      Flag()
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--boot-build-target"})
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--boot_build_target"})
          .Setter([&boot_build_target](const FlagMatch& m) -> Result<void> {
            boot_build_target = m.value;
            return {};
          }));
  std::optional<std::string> boot_branch;
  flags.emplace_back(
      Flag()
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--boot-branch"})
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--boot_branch"})
          .Setter([&boot_branch](const FlagMatch& m) -> Result<void> {
            boot_branch = m.value;
            return {};
          }));
  std::optional<std::string> boot_artifact;
  flags.emplace_back(
      Flag()
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--boot-artifact"})
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--boot_artifact"})
          .Setter([&boot_artifact](const FlagMatch& m) -> Result<void> {
            boot_artifact = m.value;
            return {};
          }));

  std::optional<std::string> ota_build_id;
  flags.emplace_back(
      Flag()
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--ota-build-id"})
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--ota_build_id"})
          .Setter([&ota_build_id](const FlagMatch& m) -> Result<void> {
            ota_build_id = m.value;
            return {};
          }));
  std::optional<std::string> ota_build_target;
  flags.emplace_back(
      Flag()
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--ota-build-target"})
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--ota_build_target"})
          .Setter([&ota_build_target](const FlagMatch& m) -> Result<void> {
            ota_build_target = m.value;
            return {};
          }));
  std::optional<std::string> ota_branch;
  flags.emplace_back(
      Flag()
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--ota-branch"})
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--ota_branch"})
          .Setter([&ota_branch](const FlagMatch& m) -> Result<void> {
            ota_branch = m.value;
            return {};
          }));

  std::optional<std::string> launch_args;
  flags.emplace_back(
      Flag()
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--launch-args"})
          .Setter([&launch_args](const FlagMatch& m) -> Result<void> {
            launch_args = m.value;
            return {};
          }));

  std::optional<std::string> system_branch;
  flags.emplace_back(
      Flag()
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--system-branch"})
          .Setter([&system_branch](const FlagMatch& m) -> Result<void> {
            system_branch = m.value;
            return {};
          }));

  std::optional<std::string> system_build_target;
  flags.emplace_back(
      Flag()
          .Alias(
              {FlagAliasMode::kFlagConsumesFollowing, "--system-build-target"})
          .Setter([&system_build_target](const FlagMatch& m) -> Result<void> {
            system_build_target = m.value;
            return {};
          }));

  std::optional<std::string> system_build_id;
  flags.emplace_back(
      Flag()
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--system-build-id"})
          .Setter([&system_build_id](const FlagMatch& m) -> Result<void> {
            system_build_id = m.value;
            return {};
          }));

  std::optional<std::string> kernel_branch;
  flags.emplace_back(
      Flag()
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--kernel-branch"})
          .Setter([&kernel_branch](const FlagMatch& m) -> Result<void> {
            kernel_branch = m.value;
            return {};
          }));

  std::optional<std::string> kernel_build_target;
  flags.emplace_back(
      Flag()
          .Alias(
              {FlagAliasMode::kFlagConsumesFollowing, "--kernel-build-target"})
          .Setter([&kernel_build_target](const FlagMatch& m) -> Result<void> {
            kernel_build_target = m.value;
            return {};
          }));

  std::optional<std::string> kernel_build_id;
  flags.emplace_back(
      Flag()
          .Alias({FlagAliasMode::kFlagConsumesFollowing, "--kernel-build-id"})
          .Setter([&kernel_build_id](const FlagMatch& m) -> Result<void> {
            kernel_build_id = m.value;
            return {};
          }));
  bool use_16k = false;
  flags.emplace_back(Flag()
                         .Alias({FlagAliasMode::kFlagExact, "--16k"})
                         .Alias({FlagAliasMode::kFlagExact, "--16K"})
                         .Alias({FlagAliasMode::kFlagExact, "--use-16k"})
                         .Alias({FlagAliasMode::kFlagExact, "--use-16K"})
                         .Setter([&use_16k](const FlagMatch&) -> Result<void> {
                           use_16k = true;
                           return {};
                         }));

  std::optional<std::string> pet_name;
  Flag pet_name_gflag = GflagsCompatFlag("pet-name");
  flags.emplace_back(
      GflagsCompatFlag("pet-name")
          .Getter([&pet_name]() { return (pet_name ? *pet_name : ""); })
          .Setter([&pet_name](const FlagMatch& match) -> Result<void> {
            pet_name = match.value;
            return {};
          }));

  CF_EXPECT(ConsumeFlags(flags, arguments));
  CF_EXPECT(arguments.empty(), "Unrecognized arguments:'"
                                   << android::base::Join(arguments, "', '")
                                   << "'");

  CF_EXPECT_EQ(parsed_flags.local_instance.is_set, true,
               "Only '--local-instance' is supported");
  auto host_dir = TempDir() + "/acloud_image_artifacts/";
  if (parsed_flags.image_download_dir) {
    host_dir =
        parsed_flags.image_download_dir.value() + "/acloud_image_artifacts/";
  }

  auto host_artifacts_path =
      CF_EXPECT(AndroidHostPath(request.Env()), "Missing host artifacts path");

  std::vector<CommandRequest> inner_requests;
  const std::string user_config_path =
      parsed_flags.config_file.value_or(CF_EXPECT(GetDefaultConfigFile()));

  AcloudConfig acloud_config = CF_EXPECT(LoadAcloudConfig(user_config_path));

  std::string fetch_command_str;
  std::string fetch_cvd_args_file;

  if (parsed_flags.local_image.given) {
    CF_EXPECT(!(system_branch || system_build_target || system_build_id),
              "--local-image incompatible with --system-* flags");
    CF_EXPECT(!(parsed_flags.bootloader.branch ||
                parsed_flags.bootloader.build_target ||
                parsed_flags.bootloader.build_id),
              "--local-image incompatible with --bootloader-* flags");
    CF_EXPECT(
        !(boot_branch || boot_build_target || boot_build_id || boot_artifact),
        "--local-image incompatible with --boot-* flags");
    CF_EXPECT(!(ota_branch || ota_build_target || ota_build_id),
              "--local-image incompatible with --ota-* flags");
  } else {
    CF_EXPECT(EnsureDirectoryExists(host_dir, 0775, /* group_name */ ""));

    // used for default branch and target when there is no input
    std::optional<BranchBuildTargetInfo> given_branch_target_info;
    if (parsed_flags.branch || parsed_flags.build_id ||
        parsed_flags.build_target) {
      auto target = parsed_flags.build_target ? *parsed_flags.build_target : "";
      auto build = parsed_flags.build_id.value_or(
          parsed_flags.branch.value_or("aosp-android-latest-release"));
      host_dir += (build + target);
    } else {
      given_branch_target_info = CF_EXPECT(GetDefaultBranchBuildTarget("git_"));
      host_dir += (given_branch_target_info->branch_str +
                   given_branch_target_info->build_target_str);
    }
    // TODO(weihsu): The default branch and target value are the
    // same as python acloud now. The only TODO item is default ID.
    // Python acloud use Android build api to query build info,
    // including the latest valid build ID. CVD acloud should follow
    // the same method by using Android build api to get build ID,
    // but it is not easy in C++.

    CommandRequestBuilder fetch_request_builder;
    fetch_request_builder.AddArguments(
        {"cvd", "fetch", "--directory", host_dir, "--default_build"});
    fetch_command_str += "--default_build=";
    if (given_branch_target_info) {
      fetch_request_builder.AddArguments({given_branch_target_info->branch_str + "/" +
                                  given_branch_target_info->build_target_str});
      fetch_command_str += (given_branch_target_info->branch_str + "/" +
                            given_branch_target_info->build_target_str);
    } else {
      auto target =
          parsed_flags.build_target ? "/" + *parsed_flags.build_target : "";
      auto build = parsed_flags.build_id.value_or(
          parsed_flags.branch.value_or("aosp-android-latest-release"));
      fetch_request_builder.AddArguments({build + target});
      fetch_command_str += (build + target);
    }
    if (system_branch || system_build_id || system_build_target) {
      auto target =
          system_build_target.value_or(parsed_flags.build_target.value_or(""));
      if (!target.empty()) {
        target = "/" + target;
      }
      auto build = system_build_id.value_or(
          system_branch.value_or("aosp-android-latest-release"));
      fetch_request_builder.AddArguments({"--system_build", build + target});
      fetch_command_str += " --system_build=" + build + target;
    }
    if (parsed_flags.bootloader.branch || parsed_flags.bootloader.build_id ||
        parsed_flags.bootloader.build_target) {
      auto target = parsed_flags.bootloader.build_target.value_or("");
      if (!target.empty()) {
        target = "/" + target;
      }
      auto build = parsed_flags.bootloader.build_id.value_or(
          parsed_flags.bootloader.branch.value_or("aosp_u-boot-mainline"));
      fetch_request_builder.AddArguments({"--bootloader_build", build + target});
      fetch_command_str += " --bootloader_build=" + build + target;
    }
    if (boot_branch || boot_build_id || boot_build_target) {
      auto target = boot_build_target.value_or("");
      if (!target.empty()) {
        target = "/" + target;
      }
      auto build = boot_build_id.value_or(
          boot_branch.value_or("aosp-android-latest-release"));
      fetch_request_builder.AddArguments({"--boot_build", build + target});
      fetch_command_str += " --boot_build=" + build + target;
    }
    if (boot_artifact) {
      CF_EXPECT(boot_branch || boot_build_target || boot_build_id,
                "--boot-artifact must combine with other --boot-* flags");
      auto target = boot_artifact.value_or("");
      fetch_request_builder.AddArguments({"--boot_artifact", target});
      fetch_command_str += " --boot_artifact=" + target;
    }
    if (ota_branch || ota_build_id || ota_build_target) {
      auto target = ota_build_target.value_or("");
      if (!target.empty()) {
        target = "/" + target;
      }
      auto build = ota_build_id.value_or(ota_branch.value_or(""));
      fetch_request_builder.AddArguments({"--otatools_build", build + target});
      fetch_command_str += " --otatools_build=" + build + target;
    }
    if (kernel_branch || kernel_build_id || kernel_build_target) {
      auto target = kernel_build_target.value_or("kernel_virt_x86_64");
      auto build = kernel_build_id.value_or(
          kernel_branch.value_or("aosp_kernel-common-android-mainline"));
      fetch_request_builder.AddArguments({"--kernel_build", build + "/" + target});
      fetch_command_str += " --kernel_build=" + build + "/" + target;
    }
    fetch_request_builder.AddEnvVar(kAndroidHostOut, host_artifacts_path);
    inner_requests.push_back(
        CF_EXPECT(std::move(fetch_request_builder).Build()));

    fetch_cvd_args_file = host_dir + "/fetch-cvd-args.txt";
    if (FileExists(fetch_cvd_args_file)) {
      std::string read_str;
      using android::base::ReadFileToString;
      CF_EXPECT(ReadFileToString(fetch_cvd_args_file.c_str(), &read_str,
                                 /* follow_symlinks */ true));
      if (read_str == fetch_command_str) {
        // same fetch cvd command, reuse original dir
        fetch_command_str = "";
        inner_requests.pop_back();
      }
    }
  }

  std::string super_image_path;
  if (parsed_flags.local_system_image) {
    // in new cvd server design, at this point,
    // we don't know which HOME is assigned by cvd start.
    // create a temporary directory to store generated
    // mix super image
    TemporaryDir my_dir;
    std::string required_paths;
    my_dir.DoNotRemove();
    super_image_path = std::string(my_dir.path) + "/" + kMixedSuperImageName;

    // combine super_image path and local_system_image path
    required_paths = super_image_path;
    required_paths += ("," + parsed_flags.local_system_image.value());

    CommandRequestBuilder mixsuperimage_request_builder;
            mixsuperimage_request_builder.AddArguments(
                {"cvd", "acloud", "mix-super-image", "--super_image"});

    if (parsed_flags.local_image.given) {
      // added image_dir to required_paths for MixSuperImage use if there is
      required_paths.append(",").append(
          parsed_flags.local_image.path.value_or(""));
      mixsuperimage_request_builder.AddEnvVar(kAndroidHostOut, host_artifacts_path);

      const auto& env = request.Env();
      auto product_out = env.find(kAndroidProductOut);
      CF_EXPECT(product_out != env.end(), "Missing " << kAndroidProductOut);
      mixsuperimage_request_builder.AddEnvVar(kAndroidProductOut, product_out->second);
    } else {
      mixsuperimage_request_builder.AddEnvVar(kAndroidHostOut, host_dir);
      mixsuperimage_request_builder.AddEnvVar(kAndroidProductOut, host_dir);
    }

    mixsuperimage_request_builder.AddArguments({required_paths});
    inner_requests.emplace_back(
        CF_EXPECT(std::move(mixsuperimage_request_builder).Build()));
  }

  CommandRequestBuilder start_request_builder;
  start_request_builder.AddArguments({"cvd", "create", "--daemon", "--undefok",
                                      "report_anonymous_usage_stats",
                                      "--report_anonymous_usage_stats", "y",
                                      "--internal_prepare_for_acloud_delete"});
  if (parsed_flags.flavor) {
    start_request_builder.AddArguments({"-config", *parsed_flags.flavor});
  }

  if (parsed_flags.local_system_image) {
    start_request_builder.AddArguments({"-super_image", super_image_path});
  }

  if (parsed_flags.local_kernel_image) {
    // kernel image has 1st priority than boot image
    struct stat statbuf {};
    std::string local_boot_image;
    std::string vendor_boot_image;
    std::string kernel_image;
    std::string initramfs_image;
    if (stat(parsed_flags.local_kernel_image.value().c_str(), &statbuf) == 0) {
      if (statbuf.st_mode & S_IFDIR) {
        // it's a directory, deal with kernel image case first
        kernel_image = FindImage(parsed_flags.local_kernel_image.value(),
                                 kKernelImageNames);
        initramfs_image = FindImage(parsed_flags.local_kernel_image.value(),
                                    kInitRamFsImageName);
        // This is the original python acloud behavior, it
        // expects both kernel and initramfs files, however,
        // there are some very old kernels that are built without
        // an initramfs.img file,
        // e.g. aosp_kernel-common-android-4.14-stable
        if (!kernel_image.empty() && !initramfs_image.empty()) {
          start_request_builder.AddArguments({"-kernel_path", kernel_image,
                                      "-initramfs_path", initramfs_image});
        } else {
          // boot.img case
          // adding boot.img and vendor_boot.img to the path
          local_boot_image = FindImage(parsed_flags.local_kernel_image.value(),
                                       kBootImageName);
          vendor_boot_image = FindImage(parsed_flags.local_kernel_image.value(),
                                        kVendorBootImageName);
          start_request_builder.AddArguments({"-boot_image", local_boot_image});
          // vendor boot image may not exist
          if (!vendor_boot_image.empty()) {
            start_request_builder.AddArguments(
                {"-vendor_boot_image", vendor_boot_image});
          }
        }
      } else if (statbuf.st_mode & S_IFREG) {
        // it's a file which directly points to boot.img
        local_boot_image = parsed_flags.local_kernel_image.value();
        start_request_builder.AddArguments({"-boot_image", local_boot_image});
      }
    }
  } else if (kernel_branch || kernel_build_id || kernel_build_target) {
    // fetch remote kernel image files
    std::string kernel_image = host_dir + "/kernel";

    // even if initramfs doesn't exist, launch_cvd will still handle it
    // correctly. We push the initramfs handler to launch_cvd stage.
    std::string initramfs_image = host_dir + "/initramfs.img";
    start_request_builder.AddArguments(
        {"-kernel_path", kernel_image, "-initramfs_path", initramfs_image});
  }

  if (launch_args) {
    start_request_builder.AddArguments(CF_EXPECT(BashTokenize(*launch_args)));
  }
  if (!acloud_config.launch_args.empty()) {
    start_request_builder.AddArguments(
        CF_EXPECT(BashTokenize(acloud_config.launch_args)));
  }
  if (pet_name) {
    const auto [group_name, instance_name] =
        CF_EXPECT(BreakDeviceName(*pet_name),
                  *pet_name << " must be a group name followed by - "
                            << "followed by an instance name.");
    std::string group_name_arg = "--";
    group_name_arg.append(selector::SelectorFlags::kGroupName)
        .append("=")
        .append(group_name);
    std::string instance_name_arg = "--";
    instance_name_arg.append(selector::SelectorFlags::kInstanceName)
        .append("=")
        .append(instance_name);
    start_request_builder.AddSelectorArguments({group_name_arg, instance_name_arg});
  }
  if (use_16k) {
    start_request_builder.AddArguments({"--use_16k"});
  }

  if (parsed_flags.local_image.given) {
    if (parsed_flags.local_image.path) {
      std::string local_image_path_str = parsed_flags.local_image.path.value();
      // Python acloud source: local_image_local_instance.py;l=81
      // this acloud flag is equal to launch_cvd flag system_image_dir
      start_request_builder.AddArguments({"-system_image_dir", local_image_path_str});
    }

    start_request_builder.AddEnvVar(kAndroidHostOut, host_artifacts_path);

    const auto& env = request.Env();
    auto product_out = env.find(kAndroidProductOut);
    CF_EXPECT(product_out != env.end(), "Missing " << kAndroidProductOut);
    start_request_builder.AddEnvVar(kAndroidProductOut, product_out->second);
  } else {
    start_request_builder.AddEnvVar(kAndroidHostOut, host_dir);
    start_request_builder.AddEnvVar(kAndroidProductOut, host_dir);
  }
  if (parsed_flags.local_instance.id) {
    start_request_builder.AddEnvVar(kCuttlefishInstanceEnvVarName,
        std::to_string(*parsed_flags.local_instance.id));
  }
  ConvertedAcloudCreateCommand ret{
      .prep_requests = std::move(inner_requests),
      .start_request = CF_EXPECT(std::move(start_request_builder).Build()),
      .fetch_command_str = fetch_command_str,
      .fetch_cvd_args_file = fetch_cvd_args_file,
      .verbose = parsed_flags.verbose,
  };
  return ret;
}

}  // namespace acloud_impl
}  // namespace cuttlefish
