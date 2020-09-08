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
//
/// \file Platform-specific runtime utilities
//
//===----------------------------------------------------------------------===//

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/ErrorOr.h>

namespace gazer
{

enum class RuntimeUtilsError : int
{
    OK,
    PLATFORM_NOT_SUPPORTED,
    NOT_IN_PATH_ENV
};


class runtime_utils_category : public std::error_category
{
public:
    virtual const char* name() const noexcept override
    {
        return "gazer::runtime_utils_category";
    }

    virtual std::string message(int value) const override;
};

llvm::ErrorOr<std::string> findProgramLocation(llvm::StringRef argvZero = {});

}

