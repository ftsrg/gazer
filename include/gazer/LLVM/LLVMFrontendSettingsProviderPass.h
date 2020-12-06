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
#ifndef GAZER_LLVMFRONTENDSETTINGSPROVIDERPASS_H
#define GAZER_LLVMFRONTENDSETTINGSPROVIDERPASS_H

#include "gazer/LLVM/LLVMFrontendSettings.h"

#include <llvm/Pass.h>

namespace gazer {

class LLVMFrontendSettingsProviderPass final : public llvm::ImmutablePass {
    LLVMFrontendSettings mSettings;
public:
    void initializePass() override;

    static char ID;
    LLVMFrontendSettingsProviderPass() : llvm::ImmutablePass(ID) { }

    virtual gazer::LLVMFrontendSettings& getSettings() { return mSettings; }
};

llvm::Pass* createLLVMFrontendSettingsProviderPass();

} // namespace gazer

#endif // GAZER_LLVMFRONTENDSETTINGSPROVIDERPASS_H
