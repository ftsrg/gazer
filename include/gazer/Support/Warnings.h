//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
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
#ifndef GAZER_SUPPORT_WARNINGS_H
#define GAZER_SUPPORT_WARNINGS_H

#include <llvm/Support/Format.h>
#include <llvm/Support/raw_ostream.h>

namespace gazer
{

template<class... Ts>
void emit_warning(const char* fmt, Ts... params)
{
    if (llvm::errs().has_colors()) {
        llvm::errs().changeColor(llvm::raw_ostream::Colors::YELLOW, /*bold=*/true, /*bg=*/false);
    }
    llvm::errs() << "warning: ";
    if (llvm::errs().has_colors()) {
        llvm::errs().resetColor();
    }
    llvm::errs() << llvm::format(fmt, params...) << "\n";
}

template<class... Ts>
void emit_error(const char* fmt, Ts... params)
{
    if (llvm::errs().has_colors()) {
        llvm::errs().changeColor(llvm::raw_ostream::Colors::RED, /*bold=*/true, /*bg=*/false);
    }
    llvm::errs() << "error: ";
    if (llvm::errs().has_colors()) {
        llvm::errs().resetColor();
    }
    llvm::errs() << llvm::format(fmt, params...) << "\n";
}

}

#endif