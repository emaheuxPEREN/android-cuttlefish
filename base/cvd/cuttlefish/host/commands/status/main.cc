/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <android-base/logging.h>
#include <gflags/gflags.h>
#include <json/value.h>

#include "cuttlefish/common/libs/fs/shared_fd.h"
#include "cuttlefish/common/libs/utils/files.h"
#include "cuttlefish/common/libs/utils/flag_parser.h"
#include "cuttlefish/common/libs/utils/result.h"
#include "cuttlefish/common/libs/utils/tee_logging.h"
#include "cuttlefish/host/libs/command_util/runner/defs.h"
#include "cuttlefish/host/libs/command_util/util.h"
#include "cuttlefish/host/libs/config/cuttlefish_config.h"

namespace cuttlefish {
namespace {

struct StatusFlags {
  std::int32_t wait_for_launcher = 5;
  std::string instance_name;
  bool print = false;
  bool all_instances = false;
  bool help_xml = false;
};

Result<StatusFlags> GetFlagValues(int argc, char** argv) {
  StatusFlags flag_values;
  std::vector<Flag> flags;
  flags.emplace_back(
      GflagsCompatFlag("wait_for_launcher", flag_values.wait_for_launcher)
          .Help("How many seconds to wait for the launcher to respond to the "
                "status command. A value of zero means wait indefinitely"));
  flags.emplace_back(
      GflagsCompatFlag("instance_name", flag_values.instance_name)
          .Help("Name of the instance to check. If not "
                "provided, DefaultInstance is used."));
  flags.emplace_back(GflagsCompatFlag("print", flag_values.print)
                         .Help("If provided, prints status and instance config "
                               "information to stdout instead of CHECK"));
  flags.emplace_back(
      GflagsCompatFlag("all_instances", flag_values.all_instances)
          .Help("List all instances status and instance config information."));
  flags.emplace_back(HelpFlag(flags));
  flags.emplace_back(HelpXmlFlag(flags, std::cout, flag_values.help_xml));
  flags.emplace_back(UnexpectedArgumentGuard());

  std::vector<std::string> args =
      ArgsToVec(argc - 1, argv + 1);  // Skip argv[0]
  CF_EXPECT(ConsumeFlags(flags, args), "Could not process command line flags.");
  return flag_values;
}

struct WebAccessUrlParam {
  std::string sig_server_addr;
  std::string webrtc_device_id;
};
std::string CalcWebAccessUrl(const WebAccessUrlParam& web_access_url_param) {
  if (!FileIsSocket(web_access_url_param.sig_server_addr)) {
    return "";
  }
  // 1443 is the port of the global webrtc "operator" service
  return std::string("https://") + "localhost" + ":" + "1443" + "/devices/" +
         web_access_url_param.webrtc_device_id + "/files" + "/client.html";
}

Json::Value PopulateDevicesInfoFromInstance(
    const CuttlefishConfig& config,
    const CuttlefishConfig::InstanceSpecific& instance_config) {
  Json::Value device_info;
  std::string webrtc_device_id = instance_config.webrtc_device_id();
  if (webrtc_device_id.empty()) {
    webrtc_device_id = instance_config.instance_name();
  }
  device_info["assembly_dir"] = config.assembly_dir();
  device_info["webrtc_device_id"] = webrtc_device_id;
  device_info["instance_dir"] = instance_config.instance_dir();
  device_info["web_access"] =
      CalcWebAccessUrl({.sig_server_addr = config.sig_server_address(),
                        .webrtc_device_id = webrtc_device_id});
  device_info["adb_serial"] = instance_config.adb_ip_and_port();
  for (int i = 0; i < instance_config.display_configs().size(); i++) {
    device_info["displays"][i] =
        std::to_string(instance_config.display_configs()[i].width) + " x " +
        std::to_string(instance_config.display_configs()[i].height) + " ( " +
        std::to_string(instance_config.display_configs()[i].dpi) + " )";
  }
  device_info["status"] = "Running";
  return device_info;
}

Result<void> CvdStatusMain(const StatusFlags& flag_values) {
  const CuttlefishConfig* config =
      CF_EXPECT(CuttlefishConfig::Get(), "Failed to obtain config object");

  Json::Value devices_info;
  auto instance_names =
      flag_values.all_instances
          ? config->instance_names()
          : std::vector<std::string>{flag_values.instance_name};
  for (int index = 0; index < instance_names.size(); index++) {
    std::string instance_name = instance_names[index];
    auto instance_config = instance_name.empty()
                               ? config->ForDefaultInstance()
                               : config->ForInstanceName(instance_name);
    SharedFD monitor_socket = CF_EXPECT(GetLauncherMonitorFromInstance(
        instance_config, flag_values.wait_for_launcher));

    LOG(INFO) << "Requesting status for instance "
              << instance_config.instance_name();
    CF_EXPECT(RunLauncherAction(monitor_socket, LauncherAction::kStatus,
                                flag_values.wait_for_launcher));

    devices_info[index] =
        PopulateDevicesInfoFromInstance(*config, instance_config);
    LOG(INFO) << "run_cvd is active for instance "
              << instance_config.instance_name();
  }
  if (flag_values.print) {
    std::cout << devices_info.toStyledString();
  }
  return {};
}

}  // namespace
}  // namespace cuttlefish

int main(int argc, char** argv) {
  ::android::base::InitLogging(argv, android::base::StderrLogger);
  cuttlefish::Result<cuttlefish::StatusFlags> flag_result =
      cuttlefish::GetFlagValues(argc, argv);
  if (!flag_result.ok()) {
    LOG(ERROR) << flag_result.error().FormatForEnv();
    return EXIT_FAILURE;
  }

  auto result = cuttlefish::CvdStatusMain(flag_result.value());
  if (!result.ok()) {
    LOG(ERROR) << result.error().FormatForEnv();
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
