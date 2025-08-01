/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <sys/stat.h>
#include <sys/types.h>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "cuttlefish/common/libs/utils/result.h"

namespace cuttlefish {
bool FileExists(const std::string& path, bool follow_symlinks = true);
Result<dev_t> FileDeviceId(const std::string& path);
Result<bool> CanHardLink(const std::string& source,
                         const std::string& destination);
inline Result<bool> CanRename(const std::string& source,
                              const std::string& destination) {
  return CanHardLink(source, destination);
}
Result<ino_t> FileInodeNumber(const std::string& path);
Result<bool> AreHardLinked(const std::string& source,
                           const std::string& destination);
Result<std::string> CreateHardLink(const std::string& target,
                                   const std::string& hardlink,
                                   bool overwrite_existing = false);
Result<void> CreateSymLink(const std::string& target, const std::string& link,
                           bool overwrite_existing = false);
Result<void> HardLinkDirecoryContentsRecursively(
    const std::string& source, const std::string& destination);
// Merges the contents of the source directory into the destination directory.
// The source directory is empty after this operation.
Result<void> MoveDirectoryContents(const std::string& source,
                                   const std::string& destination);
bool FileHasContent(const std::string& path);
Result<std::vector<std::string>> DirectoryContents(const std::string& path);
Result<std::vector<std::string>> DirectoryContentsPaths(
    const std::string& path);
bool DirectoryExists(const std::string& path, bool follow_symlinks = true);
inline bool IsDirectory(const std::string& path) {
  return DirectoryExists(path);
};
Result<void> EnsureDirectoryExists(const std::string& directory_path,
                                   mode_t mode = S_IRWXU | S_IRWXG | S_IROTH |
                                                 S_IXOTH,
                                   const std::string& group_name = "");
Result<void> ChangeGroup(const std::string& path,
                         const std::string& group_name);
bool CanAccess(const std::string& path, int mode);
bool IsDirectoryEmpty(const std::string& path);
Result<void> RecursivelyRemoveDirectory(const std::string& path);
bool Copy(const std::string& from, const std::string& to);
off_t FileSize(const std::string& path);
bool RemoveFile(const std::string& file);
Result<std::string> RenameFile(const std::string& current_filepath,
                               const std::string& target_filepath);
std::string ReadFile(const std::string& file);
Result<std::string> ReadFileContents(const std::string& filepath);
bool MakeFileExecutable(const std::string& path);
Result<std::chrono::system_clock::time_point> FileModificationTime(
    const std::string& path);
// Whether a file exists and is a unix socket
bool FileIsSocket(const std::string& path);

// acloud related API
std::string FindImage(const std::string& search_path,
                      const std::vector<std::string>& pattern);

// The returned value may contain .. or . if these are present in the path
// argument.
// path must not contain ~
std::string AbsolutePath(const std::string& path);

std::string CurrentDirectory();

struct FileSizes {
  off_t sparse_size;
  off_t disk_size;
};
FileSizes SparseFileSizes(const std::string& path);

// Find file with name |target_name| under directory |path|, return path to
// found file(if any)
Result<std::string> FindFile(const std::string& path,
                             const std::string& target_name);

Result<void> WalkDirectory(
    const std::string& dir,
    const std::function<bool(const std::string&)>& callback);

// parameter to EmulateAbsolutePath
struct InputPathForm {
  /** If nullopt, uses the process' current working dir
   *  But if there is no preceding .. or ., this field is not used.
   */
  std::optional<std::string> current_working_dir;
  /** If nullopt, use SystemWideUserHome()
   *  But, if there's no preceding ~, this field is not used.
   */
  std::optional<std::string> home_dir;
  std::string path_to_convert;
  bool follow_symlink;
};

/**
 * Returns emulated absolute path with a different process'/thread's
 * context.
 *
 * This is useful when daemon(0, 0)-started server process wants to
 * figure out a relative path that came from its client.
 *
 * The call mostly succeeds. It fails only if:
 *  home_dir isn't given so supposed to relies on the local SystemWideUserHome()
 *  but SystemWideUserHome() call fails.
 */
Result<std::string> EmulateAbsolutePath(const InputPathForm& path_info);

}  // namespace cuttlefish
