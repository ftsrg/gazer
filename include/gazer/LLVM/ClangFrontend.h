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
#ifndef GAZER_LLVM_CLANGFRONTEND_H
#define GAZER_LLVM_CLANGFRONTEND_H

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/StringRef.h>

namespace llvm
{
    class Module;
    class LLVMContext;
}

namespace gazer
{

/// Compiles a set of C and/or LLVM bitcode files using clang, links them
/// together with llvm-link and parses the resulting module.
std::unique_ptr<llvm::Module> ClangCompileAndLink(
    llvm::ArrayRef<std::string> files,
    llvm::LLVMContext& llvmContext
);

}

#endif //GAZER_CLANGFRONTEND_H
