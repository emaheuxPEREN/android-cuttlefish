/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "cuttlefish/host/commands/assemble_cvd/bootconfig_args.h"

#include <array>
#include <string>
#include <vector>

#include <android-base/parseint.h>

#include "cuttlefish/common/libs/utils/json.h"
#include "cuttlefish/host/libs/config/config_constants.h"
#include "cuttlefish/host/libs/config/cuttlefish_config.h"
#include "cuttlefish/host/libs/vm_manager/crosvm_manager.h"
#include "cuttlefish/host/libs/vm_manager/qemu_manager.h"
#include "cuttlefish/host/libs/vm_manager/vm_manager.h"

namespace cuttlefish {

using vm_manager::CrosvmManager;
using vm_manager::QemuManager;

namespace {

template <typename T>
void AppendMapWithReplacement(T* destination, const T& source) {
  for (const auto& [k, v] : source) {
    (*destination)[k] = v;
  }
}

Result<std::unordered_map<std::string, std::string>> ConsoleBootconfig(
    const CuttlefishConfig::InstanceSpecific& instance) {
  std::unordered_map<std::string, std::string> bootconfig_args;
  if (instance.console()) {
    bootconfig_args["androidboot.console"] = instance.console_dev();
    bootconfig_args["androidboot.serialconsole"] = "1";
  } else {
    // Specify an invalid path under /dev, so the init process will disable the
    // console service due to the console not being found. On physical devices,
    // *and on older kernels* it is enough to not specify androidboot.console=
    // *and* not specify the console= kernel command line parameter, because
    // the console and kernel dmesg are muxed. However, on cuttlefish, we don't
    // need to mux, and would prefer to retain the kernel dmesg logging, so we
    // must work around init falling back to the check for /dev/console (which
    // we'll always have).
    // bootconfig_args["androidboot.console"] = "invalid";
    // The bug above has been fixed in Android 14 and later so we can just
    // specify androidboot.serialconsole=0 instead.
    bootconfig_args["androidboot.serialconsole"] = "0";
  }
  return bootconfig_args;
}

}  // namespace

Result<std::unordered_map<std::string, std::string>> BootconfigArgsFromConfig(
    const CuttlefishConfig& config,
    const CuttlefishConfig::InstanceSpecific& instance) {
  std::unordered_map<std::string, std::string> bootconfig_args;

  AppendMapWithReplacement(&bootconfig_args,
                           CF_EXPECT(ConsoleBootconfig(instance)));

  auto vmm =
      vm_manager::GetVmManager(config.vm_manager(), instance.target_arch());
  AppendMapWithReplacement(&bootconfig_args,
                           CF_EXPECT(vmm->ConfigureBootDevices(instance)));

  AppendMapWithReplacement(&bootconfig_args,
                           CF_EXPECT(vmm->ConfigureGraphics(instance)));

  bootconfig_args["androidboot.serialno"] = instance.serial_number();
  bootconfig_args["androidboot.ddr_size"] =
      std::to_string(instance.ddr_mem_mb()) + "MB";

  // TODO(b/131884992): update to specify multiple once supported.
  const auto display_configs = instance.display_configs();
  if (!display_configs.empty()) {
    bootconfig_args["androidboot.lcd_density"] =
        std::to_string(display_configs[0].dpi);
  }

  bootconfig_args["androidboot.setupwizard_mode"] = instance.setupwizard_mode();

  bootconfig_args["androidboot.enable_bootanimation"] =
      std::to_string(instance.enable_bootanimation());

  if (!instance.guest_enforce_security()) {
    bootconfig_args["androidboot.selinux"] = "permissive";
  }

  if (instance.tombstone_receiver_port()) {
    bootconfig_args["androidboot.vsock_tombstone_port"] =
        std::to_string(instance.tombstone_receiver_port());
  }

  if (instance.openthread_node_id()) {
    bootconfig_args["androidboot.openthread_node_id"] =
        std::to_string(instance.openthread_node_id());
  }

  const auto enable_confui = (config.vm_manager() == VmmMode::kQemu ? 0 : 1);
  bootconfig_args["androidboot.enable_confirmationui"] =
      std::to_string(enable_confui);

  if (instance.audiocontrol_server_port()) {
    bootconfig_args["androidboot.vendor.audiocontrol.server.cid"] =
        std::to_string(instance.vsock_guest_cid());
    bootconfig_args["androidboot.vendor.audiocontrol.server.port"] =
        std::to_string(instance.audiocontrol_server_port());
  }

  if (!instance.enable_audio()) {
    bootconfig_args["androidboot.audio.tinyalsa.ignore_output"] = "true";
    bootconfig_args["androidboot.audio.tinyalsa.simulate_input"] = "true";
  }

  if (instance.camera_server_port()) {
    bootconfig_args["androidboot.vsock_camera_port"] =
        std::to_string(instance.camera_server_port());
    bootconfig_args["androidboot.vsock_camera_cid"] =
        std::to_string(instance.vsock_guest_cid());
  }

  if (instance.lights_server_port()) {
    bootconfig_args["androidboot.vsock_lights_port"] =
        std::to_string(instance.lights_server_port());
    bootconfig_args["androidboot.vsock_lights_cid"] =
        std::to_string(instance.vsock_guest_cid());
  }

  if (instance.enable_modem_simulator() &&
      !instance.modem_simulator_ports().empty()) {
    bootconfig_args["androidboot.modem_simulator_ports"] =
        instance.modem_simulator_ports();
  }

  // Once all Cuttlefish kernel versions are at least 5.15, filename encryption
  // will not need to be set conditionally. HCTR2 will always be available.
  // At that point fstab.cf.f2fs.cts and fstab.cf.ext4.cts can be removed.
  std::string fstab_suffix = fmt::format("cf.{}.{}", instance.userdata_format(),
                                         instance.filename_encryption_mode());

  bootconfig_args["androidboot.fstab_suffix"] = fstab_suffix;

  bootconfig_args["androidboot.wifi_mac_prefix"] =
      std::to_string(instance.wifi_mac_prefix());

  // Non-native architecture implies a significantly slower execution speed, so
  // set a large timeout multiplier.
  if (!IsHostCompatible(instance.target_arch())) {
    bootconfig_args["androidboot.hw_timeout_multiplier"] = "50";
  } else {
    // Even on native architecture, Cuttlefish is still slower than physical
    // devices in CI environments, so add a small timeout multiplier.
    bootconfig_args["androidboot.hw_timeout_multiplier"] = "3";
  }

  // TODO(b/217564326): improve this checks for a hypervisor in the VM.
  if (instance.target_arch() == Arch::X86 ||
      instance.target_arch() == Arch::X86_64) {
    bootconfig_args["androidboot.hypervisor.version"] =
        "cf-" + ToString(config.vm_manager());
    bootconfig_args["androidboot.hypervisor.vm.supported"] = "1";
  } else {
    bootconfig_args["androidboot.hypervisor.vm.supported"] = "0";
  }
  bootconfig_args["androidboot.hypervisor.protected_vm.supported"] = "0";
  if (!instance.kernel_path().empty()) {
    bootconfig_args["androidboot.kernel_hotswapped"] = "1";
  }
  if (!instance.initramfs_path().empty()) {
    bootconfig_args["androidboot.ramdisk_hotswapped"] = "1";
  }

  const auto& secure_hals = CF_EXPECT(config.secure_hals());
  if (secure_hals.count(SecureHal::kGuestKeymintInsecure)) {
    bootconfig_args["androidboot.vendor.apex.com.android.hardware.keymint"] =
        "com.android.hardware.keymint.rust_nonsecure";
  } else if (secure_hals.count(SecureHal::kGuestKeymintTrustyInsecure)) {
    bootconfig_args["androidboot.vendor.apex.com.android.hardware.keymint"] =
        "com.android.hardware.keymint.rust_cf_guest_trusty_nonsecure";
  } else {
    bootconfig_args["androidboot.vendor.apex.com.android.hardware.keymint"] =
        "com.android.hardware.keymint.rust_cf_remote";
  }

  // Preemptive for when we set up the HAL to be runtime selectable
  bootconfig_args["androidboot.vendor.apex.com.android.hardware.gatekeeper"] =
      secure_hals.count(SecureHal::kGuestGatekeeperInsecure)
          ? "com.android.hardware.gatekeeper.nonsecure"
          : "com.android.hardware.gatekeeper.cf_remote";

  // jcardsimulator
  if (secure_hals.count(SecureHal::kGuestStrongboxInsecure)) {
    bootconfig_args
        ["androidboot.vendor.apex.com.android.hardware.secure_element"] =
            "com.android.hardware.secure_element_jcardsim";
    bootconfig_args["androidboot.vendor.apex.com.android.hardware.strongbox"] =
        "com.android.hardware.strongbox";
  } else {
    bootconfig_args
        ["androidboot.vendor.apex.com.android.hardware.secure_element"] =
            "com.android.hardware.secure_element";
    bootconfig_args["androidboot.vendor.apex.com.android.hardware.strongbox"] =
        "none";
  }

  bootconfig_args
      ["androidboot.vendor.apex.com.android.hardware.graphics.composer"] =
          instance.hwcomposer() == kHwComposerDrm
              ? "com.android.hardware.graphics.composer.drm_hwcomposer"
              : "com.android.hardware.graphics.composer.ranchu";

  if (config.vhal_proxy_server_port()) {
    bootconfig_args["androidboot.vhal_proxy_server_port"] =
        std::to_string(config.vhal_proxy_server_port());
    int32_t instance_id;
    CF_EXPECT(android::base::ParseInt(instance.id(), &instance_id),
              "instance id: " << instance.id() << " is not a valid int");
    // The static ethernet IP address assigned for the guest.
    bootconfig_args["androidboot.auto_eth_guest_addr"] =
        fmt::format("192.168.98.{}", instance_id + 2);
  }

  if (config.virtio_mac80211_hwsim()) {
    bootconfig_args["androidboot.wifi_impl"] = "mac80211_hwsim_virtio";
  } else {
    bootconfig_args["androidboot.wifi_impl"] = "virt_wifi";
  }

  if (!instance.vcpu_config_path().empty()) {
    auto vcpu_config_json =
        CF_EXPECT(LoadFromFile(instance.vcpu_config_path()));

    const auto guest_soc =
        CF_EXPECT(GetValue<std::string>(vcpu_config_json, {"guest_soc"}));

    bootconfig_args["androidboot.guest_soc.model"] = guest_soc;
  }

  std::vector<std::string> args = instance.extra_bootconfig_args();

  LOG(DEBUG) << "Parsing extra_bootconfig_args of size:" << args.size()
             << "; Contents: " << android::base::Join(args, "\n");

  for (const std::string& kv : args) {
    if (kv.empty()) {
      continue;
    }
    const auto& parts = android::base::Split(kv, "=");
    CF_EXPECT_EQ(parts.size(), 2,
                 "Failed to parse --extra_bootconfig_args: \"" << kv << "\"");
    bootconfig_args[parts[0]] = parts[1];
  }

  return bootconfig_args;
}

Result<std::string> BootconfigArgsString(
    const std::unordered_map<std::string, std::string>& args,
    const std::string& separator) {
  std::vector<std::string> combined_args;
  for (const auto& [k, v] : args) {
    CF_EXPECT(!v.empty(), "Found empty bootconfig value for " << k);
    combined_args.push_back(k + "=" + v);
  }
  return android::base::Join(combined_args, separator);
}

}  // namespace cuttlefish
