//
// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cuttlefish/host/commands/run_cvd/launch/root_canal.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <fruit/component.h>
#include <fruit/fruit_forward_decls.h>
#include <fruit/macro.h>

#include "cuttlefish/common/libs/utils/result.h"
#include "cuttlefish/common/libs/utils/subprocess.h"
#include "cuttlefish/host/commands/run_cvd/launch/log_tee_creator.h"
#include "cuttlefish/host/libs/config/cuttlefish_config.h"
#include "cuttlefish/host/libs/config/known_paths.h"
#include "cuttlefish/host/libs/feature/command_source.h"
#include "cuttlefish/host/libs/feature/feature.h"

namespace cuttlefish {
namespace {

class RootCanal : public CommandSource {
 public:
  INJECT(RootCanal(const CuttlefishConfig& config,
                   const CuttlefishConfig::InstanceSpecific& instance,
                   LogTeeCreator& log_tee))
      : config_(config), instance_(instance), log_tee_(log_tee) {}

  // CommandSource
  Result<std::vector<MonitorCommand>> Commands() override {
    // Create the rootcanal command with the process_restarter
    // as runner to restart rootcanal when it crashes.
    Command rootcanal(ProcessRestarterBinary());
    rootcanal.AddParameter("-when_killed");
    rootcanal.AddParameter("-when_dumped");
    rootcanal.AddParameter("-when_exited_with_failure");
    rootcanal.AddParameter("--");
    rootcanal.AddParameter(RootCanalBinary());

    // Port configuration.
    rootcanal.AddParameter("--test_port=", config_.rootcanal_test_port());
    rootcanal.AddParameter("--hci_port=", config_.rootcanal_hci_port());
    rootcanal.AddParameter("--link_port=", config_.rootcanal_link_port());
    rootcanal.AddParameter("--link_ble_port=",
                           config_.rootcanal_link_ble_port());

    // Add parameters from passthrough option --rootcanal-args
    for (auto const& arg : config_.rootcanal_args()) {
      rootcanal.AddParameter(arg);
    }

    // Add command for forwarding the HCI port to a vsock server.
    Command hci_vsock_proxy(SocketVsockProxyBinary());
    hci_vsock_proxy.AddParameter("--server_type=vsock");
    hci_vsock_proxy.AddParameter("--server_vsock_id=",
                                 instance_.vsock_guest_cid());
    hci_vsock_proxy.AddParameter("--server_vsock_port=",
                                 config_.rootcanal_hci_port());
    hci_vsock_proxy.AddParameter("--client_type=tcp");
    hci_vsock_proxy.AddParameter("--client_tcp_host=127.0.0.1");
    hci_vsock_proxy.AddParameter("--client_tcp_port=",
                                 config_.rootcanal_hci_port());

    // Add command for forwarding the test port to a vsock server.
    Command test_vsock_proxy(SocketVsockProxyBinary());
    test_vsock_proxy.AddParameter("--server_type=vsock");
    test_vsock_proxy.AddParameter("--server_vsock_id=",
                                  instance_.vsock_guest_cid());
    test_vsock_proxy.AddParameter("--server_vsock_port=",
                                  config_.rootcanal_test_port());
    test_vsock_proxy.AddParameter("--client_type=tcp");
    test_vsock_proxy.AddParameter("--client_tcp_host=127.0.0.1");
    test_vsock_proxy.AddParameter("--client_tcp_port=",
                                  config_.rootcanal_test_port());

    // Add command for forwarding the link layer port to a vsock server.
    Command link_vsock_proxy(SocketVsockProxyBinary());
    link_vsock_proxy.AddParameter("--server_type=vsock");
    link_vsock_proxy.AddParameter("--server_vsock_id=",
                                  instance_.vsock_guest_cid());
    link_vsock_proxy.AddParameter("--server_vsock_port=",
                                  config_.rootcanal_link_port());
    link_vsock_proxy.AddParameter("--client_type=tcp");
    link_vsock_proxy.AddParameter("--client_tcp_host=127.0.0.1");
    link_vsock_proxy.AddParameter("--client_tcp_port=",
                                  config_.rootcanal_link_port());

    // Add command for forwarding the link layer ble port to a vsock server.
    Command link_ble_vsock_proxy(SocketVsockProxyBinary());
    link_ble_vsock_proxy.AddParameter("--server_type=vsock");
    link_ble_vsock_proxy.AddParameter("--server_vsock_id=",
                                      instance_.vsock_guest_cid());
    link_ble_vsock_proxy.AddParameter("--server_vsock_port=",
                                      config_.rootcanal_link_ble_port());
    link_ble_vsock_proxy.AddParameter("--client_type=tcp");
    link_ble_vsock_proxy.AddParameter("--client_tcp_host=127.0.0.1");
    link_ble_vsock_proxy.AddParameter("--client_tcp_port=",
                                      config_.rootcanal_link_ble_port());

    std::vector<MonitorCommand> commands;
    commands.emplace_back(
        CF_EXPECT(log_tee_.CreateFullLogTee(rootcanal, "rootcanal")));
    commands.emplace_back(std::move(rootcanal));
    commands.emplace_back(std::move(hci_vsock_proxy));
    commands.emplace_back(std::move(test_vsock_proxy));
    commands.emplace_back(std::move(link_vsock_proxy));
    commands.emplace_back(std::move(link_ble_vsock_proxy));
    return commands;
  }

  // SetupFeature
  std::string Name() const override { return "RootCanal"; }
  bool Enabled() const override {
    return config_.enable_host_bluetooth_connector() && instance_.start_rootcanal();
  }

 private:
  std::unordered_set<SetupFeature*> Dependencies() const override { return {}; }
  Result<void> ResultSetup() override { return {}; }

  const CuttlefishConfig& config_;
  const CuttlefishConfig::InstanceSpecific& instance_;
  LogTeeCreator& log_tee_;
};

}  // namespace

fruit::Component<
    fruit::Required<const CuttlefishConfig,
                    const CuttlefishConfig::InstanceSpecific, LogTeeCreator>>
RootCanalComponent() {
  return fruit::createComponent()
      .addMultibinding<CommandSource, RootCanal>()
      .addMultibinding<SetupFeature, RootCanal>();
}

}  // namespace cuttlefish
