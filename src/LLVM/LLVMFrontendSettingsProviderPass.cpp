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
#include "gazer/LLVM/LLVMFrontendSettingsProviderPass.h"

using namespace gazer;

void gazer::LLVMFrontendSettingsProviderPass::initializePass() {
    mSettings = gazer::LLVMFrontendSettings::initFromCommandLine();
}

char gazer::LLVMFrontendSettingsProviderPass::ID;

static llvm::RegisterPass<LLVMFrontendSettingsProviderPass>
    X("gazer-settings-provider", "Gazer Settings Provider Pass", false, true);

llvm::Pass* gazer::createLLVMFrontendSettingsProviderPass()
{
    return new LLVMFrontendSettingsProviderPass();
}