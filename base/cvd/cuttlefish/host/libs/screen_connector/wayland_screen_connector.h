/*
 * Copyright (C) 2019 The Android Open Source Project
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

#pragma once

#include <memory>

#include <fruit/fruit.h>

#include "cuttlefish/host/libs/screen_connector/screen_connector_common.h"
#include "cuttlefish/host/libs/wayland/wayland_server.h"

namespace cuttlefish {

class WaylandScreenConnector {
 public:
  struct FramesFd {};
  struct FramesAreRgba {};
  INJECT(WaylandScreenConnector(ANNOTATED(FramesFd, int) frames_fd,
                                ANNOTATED(FramesAreRgba, bool)
                                    frames_are_rgba));

  void SetFrameCallback(GenerateProcessedFrameCallbackImpl frame_callback);

  void SetDisplayEventCallback(DisplayEventCallback event_callback);

 private:
  std::unique_ptr<wayland::WaylandServer> server_;
};

}  // namespace cuttlefish
