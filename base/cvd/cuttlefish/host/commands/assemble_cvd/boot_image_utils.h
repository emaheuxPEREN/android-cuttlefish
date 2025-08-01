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

#include <optional>
#include <string>

#include "cuttlefish/common/libs/utils/result.h"
#include "cuttlefish/host/commands/assemble_cvd/disk/generate_persistent_bootconfig.h"
#include "cuttlefish/host/libs/avb/avb.h"

namespace cuttlefish {

Result<void> RepackBootImage(const Avb& avb,
                             const std::string& new_kernel_path,
                             const std::string& boot_image_path,
                             const std::string& new_boot_image_path,
                             const std::string& build_dir);
bool RepackVendorBootImage(const std::string& new_ramdisk_path,
                           const std::string& vendor_boot_image_path,
                           const std::string& new_vendor_boot_image_path,
                           const std::string& unpack_dir,
                           bool bootconfig_supported);
bool RepackVendorBootImageWithEmptyRamdisk(
    const std::string& vendor_boot_image_path,
    const std::string& new_vendor_boot_image_path,
    const std::string& unpack_dir, bool bootconfig_supported);

Result<std::string> UnpackBootImage(const std::string& boot_image_path,
                                    const std::string& unpack_dir);

bool UnpackVendorBootImageIfNotUnpacked(
    const std::string& vendor_boot_image_path, const std::string& unpack_dir);
void RepackGem5BootImage(const std::string& initrd_path,
                         const std::optional<BootConfigPartition>&,
                         const std::string& unpack_dir,
                         const std::string& input_ramdisk_path);
Result<std::string> ReadAndroidVersionFromBootImage(
    const std::string& boot_image_path);

void UnpackRamdisk(const std::string& original_ramdisk_path,
                   const std::string& ramdisk_stage_dir);
void PackRamdisk(const std::string& ramdisk_stage_dir,
                 const std::string& output_ramdisk);
}
