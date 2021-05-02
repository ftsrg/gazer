//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2020 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#include "gazer/Support/Runtime.h"

#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringExtras.h>

#include <llvm/Support/Process.h>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>

#ifdef __linux__
#include <unistd.h>
#endif

using namespace gazer;

std::string runtime_utils_category::message(int value) const
{
    switch (static_cast<RuntimeUtilsError>(value)) {
        case RuntimeUtilsError::OK:
            return "";
        case RuntimeUtilsError::PLATFORM_NOT_SUPPORTED:
            return "Current platform is not supported";
        case RuntimeUtilsError::NOT_IN_PATH_ENV:
            return "Executable was not found in PATH";
        default:
            break;
    }

    return "";
}

static llvm::ErrorOr<std::string> read_symlink(llvm::StringRef path)
{
    // TODO(sallaigy): Remove this if we ever start using std::filesystem instead of llvm::sys::fs.
#ifdef __linux__
    size_t size = 128;
    std::vector<char> buffer(size, '\0');

    while (true) {
        auto ret = ::readlink(path.data(), buffer.data(), size);
        if (ret < 0) {
            std::error_code ec(errno, std::system_category());
            return ec;
        }

        if (ret < static_cast<ssize_t>(size)) {
            return std::string(buffer.data(), static_cast<std::string::size_type>(ret));
        }
        size *= 2;
        buffer.reserve(size);
    }
#else
    return std::error_code(static_cast<int>(RuntimeUtilsError::PLATFORM_NOT_SUPPORTED), runtime_utils_category());
#endif
}

llvm::ErrorOr<std::string> gazer::findProgramLocation(llvm::StringRef argvZero)
{
    llvm::ErrorOr<std::string> result = std::error_code(static_cast<int>(RuntimeUtilsError::PLATFORM_NOT_SUPPORTED), runtime_utils_category());
#ifdef __linux__
    // See if we can read /proc/self/exe on Linux
    result = read_symlink("/proc/self/exe");
    if (result) {
        return result;
    }
#endif

    // Try a heuristic based on argv[0], if supplied
    if (argvZero.empty()) {
        return result;
    }

    llvm::SmallString<64> path(argvZero);

    if (llvm::sys::path::is_absolute(argvZero) || llvm::sys::path::is_relative(argvZero)) {
        if (auto ec = llvm::sys::fs::real_path(argvZero, path); ec) {
            return ec;
        }

        return path.str();
    }

    // See if we can look it up in the PATH
    auto findInPathResult = llvm::sys::Process::FindInEnvPath("PATH", argvZero);
    if (!findInPathResult) {
        return std::error_code(static_cast<int>(RuntimeUtilsError::NOT_IN_PATH_ENV), runtime_utils_category());
    }

    return findInPathResult.getValue();
}
