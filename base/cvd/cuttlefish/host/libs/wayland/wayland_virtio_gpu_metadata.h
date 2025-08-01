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

#pragma once

#include <stdint.h>

#include <wayland-server-core.h>

#include "cuttlefish/host/libs/wayland/wayland_surfaces.h"

namespace wayland {

// Binds the virtio gpu metadata interface to the given wayland server.
void BindVirtioGpuMetadataInterface(wl_display* display, Surfaces* surfaces);

}  // namespace wayland