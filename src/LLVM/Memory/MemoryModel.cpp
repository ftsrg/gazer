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

#include "gazer/LLVM/Memory/MemoryModel.h"

#include "gazer/Core/LiteralExpr.h"
#include "gazer/LLVM/Memory/MemorySSA.h"
#include "gazer/LLVM/LLVMFrontendSettingsProviderPass.h"

#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>

#include <llvm/Support/CommandLine.h>

using namespace gazer;
using namespace llvm;

cl::opt<MemoryModelSetting> MemoryModelOpt("memory", cl::desc("Memory model to use:"),
                                           cl::values(
                                               clEnumValN(MemoryModelSetting::Flat, "flat", "Bit-precise flat memory model"),
                                               clEnumValN(MemoryModelSetting::Havoc, "havoc", "Dummy havoc model")
                                           ),
                                           cl::init(MemoryModelSetting::Flat),
                                           cl::cat(IrToCfaCategory)
);

// LLVM pass implementation
//===----------------------------------------------------------------------===//

char MemoryModelWrapperPass::ID;

void MemoryModelWrapperPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<LLVMFrontendSettingsProviderPass>();

    // TODO
    LLVMFrontendSettings settings = LLVMFrontendSettings::initFromCommandLine();
    switch (settings.memoryModel) {
        case MemoryModelSetting::Flat:
            au.addRequired<llvm::DominatorTreeWrapperPass>();
        case MemoryModelSetting::Havoc:
            break;
        default:
            llvm_unreachable("Unknown memory model setting!");
    }

    au.setPreservesAll();
}

bool MemoryModelWrapperPass::runOnModule(llvm::Module& module)
{
    auto& settings = getAnalysis<LLVMFrontendSettingsProviderPass>()
        .getSettings();
    switch (settings.memoryModel) {
        case MemoryModelSetting::Flat: {
            auto dominators = [this](llvm::Function& function) -> llvm::DominatorTree& {
                return getAnalysis<llvm::DominatorTreeWrapperPass>(function).getDomTree();
            };

            mMemoryModel = CreateFlatMemoryModel(mContext, settings, module, dominators);
            break;
        }
        case MemoryModelSetting::Havoc: {
            mMemoryModel = CreateHavocMemoryModel(mContext);
            break;
        }
    }

    assert(mMemoryModel != nullptr && "Unknown memory model setting!");

    return true;
}

MemoryModelSetting& gazer::getMemoryModel() {
    return MemoryModelOpt.getValue();
}