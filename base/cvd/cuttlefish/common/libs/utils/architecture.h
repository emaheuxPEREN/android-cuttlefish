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
#pragma once

#include <ostream>
#include <string>

namespace cuttlefish {

enum class Arch {
  Arm,
  Arm64,
  RiscV64,
  X86,
  X86_64,
};

const std::string& HostArchStr();
Arch HostArch();
bool IsHostCompatible(Arch arch);

std::ostream& operator<<(std::ostream&, Arch);

}  // namespace cuttlefish
