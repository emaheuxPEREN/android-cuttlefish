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

#include <stdlib.h>
#include <iostream>
//
#include <android-base/logging.h>
#include <gflags/gflags.h>
//
#include "cuttlefish/common/libs/utils/subprocess.h"
#include "cuttlefish/common/libs/utils/subprocess_managed_stdio.h"
#include "cuttlefish/host/libs/config/cuttlefish_config.h"

std::string GetControlSocketPath(const cuttlefish::CuttlefishConfig& config) {
  return config.ForDefaultInstance().CrosvmSocketPath();
}

static constexpr char kUsageMessage[] =
    "<key> [value]\n"
    "Excluding the value will enumerate the possible values to set\n"
    "\n"
    "\"status [value]\" - battery status: "
    "unknown/charging/discharging/notcharging/full\n"
    "\"health [value]\" - battery health\n"
    "\"present [value]\" - battery present: 1 or 0\n"
    "\"capacity [value]\" - battery capacity: 0 to 100\n"
    "\"aconline [value]\" - battery ac online: 1 or 0\n";

int status() {
  std::cout
      << "health status [value]\n"
         "\"value\" - unknown, charging, discharging, notcharging, full\n";
  return 0;
}

int health() {
  std::cout << "health health [value]\n"
               "\"value\" - unknown, good, overheat, dead, overvoltage, "
               "unexpectedfailure,\n"
               "          cold, watchdogtimerexpire, safetytimerexpire, "
               "overcurrent\n";
  return 0;
}

int present() {
  std::cout << "health present [value]\n"
               "\"value\" - 1, 0\n";
  return 0;
}

int capacity() {
  std::cout << "health capacity [value]\n"
               "\"value\" - 0 to 100\n";
  return 0;
}

int aconline() {
  std::cout << "health aconline [value]\n"
               "\"value\" - 1, 0\n";
  return 0;
}

int usage() {
  std::cout << "health " << kUsageMessage;
  return 1;
}

int main(int argc, char** argv) {
  ::android::base::InitLogging(argv, android::base::StderrLogger);
  gflags::SetUsageMessage(kUsageMessage);

  auto config = cuttlefish::CuttlefishConfig::Get();
  if (!config) {
    LOG(ERROR) << "Failed to obtain config object";
    return 1;
  }
  // TODO(b/260649774): Consistent executable API for selecting an instance
  auto instance = config->ForInstance(cuttlefish::GetInstance());

  if (argc != 2 && argc != 3) {
    return usage();
  }

  std::string key = argv[1];
  std::string value = "";
  if (argc == 3) {
    value = argv[2];
  }

  if (argc == 2 || value == "--help" || value == "-h" || value == "help") {
    if (key == "status") {
      return status();
    } else if (key == "health") {
      return health();
    } else if (key == "present") {
      return present();
    } else if (key == "capacity") {
      return capacity();
    } else if (key == "aconline") {
      return aconline();
    } else {
      return usage();
    }
  }

  cuttlefish::Command command =
      cuttlefish::Command(instance.crosvm_binary())
          .AddParameter("battery")
          .AddParameter("goldfish")
          .AddParameter(key)
          .AddParameter(value)
          .AddParameter(GetControlSocketPath(*config));

  cuttlefish::Result<std::string> res =
      cuttlefish::RunAndCaptureStdout(std::move(command));
  if (res.ok()) {
    return 0;
  } else {
    LOG(ERROR) << "goldfish battery failed: " << res.error().FormatForEnv();
    return 1;
  }
}
