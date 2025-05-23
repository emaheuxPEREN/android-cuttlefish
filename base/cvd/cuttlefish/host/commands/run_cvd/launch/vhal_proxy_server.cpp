//
// Copyright (C) 2024 The Android Open Source Project
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

#include "cuttlefish/host/commands/run_cvd/launch/vhal_proxy_server.h"

#include <linux/vm_sockets.h>

#include <optional>

#include <fmt/core.h>

#include "cuttlefish/common/libs/fs/shared_fd.h"
#include "cuttlefish/common/libs/utils/subprocess.h"
#include "cuttlefish/host/libs/config/command_source.h"
#include "cuttlefish/host/libs/config/cuttlefish_config.h"
#include "cuttlefish/host/libs/config/known_paths.h"
#include "cuttlefish/host/libs/vhal_proxy_server/vhal_proxy_server_eth_addr.h"

namespace cuttlefish {

std::optional<MonitorCommand> VhalProxyServer(
    const CuttlefishConfig& config,
    const CuttlefishConfig::InstanceSpecific& instance) {
  if (!instance.start_vhal_proxy_server()) {
    return {};
  }
  int port = config.vhal_proxy_server_port();
  Command command = Command(VhalProxyServerBinary())
                        .AddParameter(VhalProxyServerConfig())
                        .AddParameter(fmt::format(
                            "{}:{}", vhal_proxy_server::kEthAddr, port));
  if (instance.vhost_user_vsock()) {
    command.AddParameter(
        fmt::format("unix://{}", SharedFD::GetVhostUserVsockServerAddr(
                                     port, instance.vsock_guest_cid())));
  } else {
    command.AddParameter(fmt::format("vsock:{}:{}", VMADDR_CID_HOST, port));
  }
  return command;
}

}  // namespace cuttlefish
