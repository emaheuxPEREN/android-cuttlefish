/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0f
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>

#include <memory>

#include <fruit/fruit.h>

#include "cuttlefish/host/libs/confui/host_server.h"
#include "cuttlefish/host/libs/input_connector/input_connector.h"

namespace cuttlefish {
namespace confui {
enum class ConfUiKeys : std::uint32_t { Confirm = 7, Cancel = 8 };

/**
 * webrtc will deliver the user inputs from their client
 * to this class object
 */
class HostVirtualInput : public InputConnector {
 public:
  INJECT(HostVirtualInput(HostServer& host_server, HostModeCtrl& host_mode_ctrl,
                          InputConnector& android_mode_input));

  ~HostVirtualInput() = default;

  void UserAbortEvent();

  // guarantees that if this returns true, it is confirmation UI mode
  bool IsConfUiActive();

  HostServer& host_server() { return host_server_; }

  // InputConnector implementation.
  std::unique_ptr<EventSink> CreateSink() override;

 private:
  HostServer& host_server_;
  HostModeCtrl& host_mode_ctrl_;
  InputConnector& android_mode_input_;
};
}  // namespace confui
}  // namespace cuttlefish
