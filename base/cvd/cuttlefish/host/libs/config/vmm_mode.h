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
#include <string_view>

#include <fmt/ostream.h>

#include "cuttlefish/common/libs/utils/result.h"

namespace cuttlefish {

enum class VmmMode {
  kUnknown,
  kCrosvm,
  kGem5,
  kQemu,
};

std::ostream& operator<<(std::ostream&, VmmMode);
std::string ToString(VmmMode mode);
Result<VmmMode> ParseVmm(std::string_view);

}  // namespace cuttlefish

#if FMT_VERSION >= 90000
template <>
struct fmt::formatter<cuttlefish::VmmMode> : ostream_formatter {};
#endif
