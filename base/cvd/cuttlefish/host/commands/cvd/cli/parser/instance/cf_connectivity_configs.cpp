/*
 * Copyright (C) 2024 The Android Open Source Project
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
#include "cuttlefish/host/commands/cvd/cli/parser/instance/cf_connectivity_configs.h"

#include <string>
#include <vector>

#include "cuttlefish/host/commands/cvd/cli/parser/cf_configs_common.h"
#include "cuttlefish/host/commands/cvd/cli/parser/load_config.pb.h"

namespace cuttlefish {

using cvd::config::EnvironmentSpecification;
using cvd::config::Instance;

static std::string VsockGuestGroup(const Instance& instance) {
  return instance.connectivity().vsock().guest_group();
}

std::vector<std::string> GenerateConnectivityFlags(
    const EnvironmentSpecification& cfg) {
  return std::vector<std::string>{
      GenerateInstanceFlag("vsock_guest_group", cfg, VsockGuestGroup),
  };
}

}  // namespace cuttlefish
