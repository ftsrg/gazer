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
#ifndef GAZER_LLVM_TYPETRANSLATOR_H
#define GAZER_LLVM_TYPETRANSLATOR_H

#include "gazer/Core/Type.h"
#include "gazer/LLVM/LLVMFrontendSettings.h"

#include <llvm/IR/Type.h>
#include "llvm/IR/DerivedTypes.h"

namespace gazer
{

class MemoryTypeTranslator;

class LLVMTypeTranslator final
{
public:
    LLVMTypeTranslator(
        MemoryTypeTranslator& memoryTypes, const LLVMFrontendSettings& settings)
        : mMemoryTypes(memoryTypes), mSettings(settings)
    {}

    gazer::Type& get(const llvm::Type* type);

private:
    MemoryTypeTranslator& mMemoryTypes;
    const LLVMFrontendSettings& mSettings;
};

} // end namespace gazer

#endif