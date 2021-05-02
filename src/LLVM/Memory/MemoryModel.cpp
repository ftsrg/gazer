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

#include "gazer/Core/LiteralExpr.h"
#include "gazer/LLVM/Memory/MemorySSA.h"
#include "gazer/LLVM/Memory/MemoryModel.h"

#include <llvm/Transforms/Utils/UnifyFunctionExitNodes.h>

using namespace gazer;

// LLVM pass implementation
//===----------------------------------------------------------------------===//

char MemoryModelWrapperPass::ID;

void MemoryModelWrapperPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    switch (mSettings.memoryModel) {
        case MemoryModelSetting::Flat:
            au.addRequired<llvm::DominatorTreeWrapperPass>();
        case MemoryModelSetting::Havoc:
            break;
        default:
            llvm_unreachable("Unknown memory model setting!");
    }

    au.setPreservesAll();
}

bool MemoryModelWrapperPass::runOnModule(llvm::Module& llvmModule)
{
    switch (mSettings.memoryModel) {
        case MemoryModelSetting::Flat: {
            auto dominators = [this](llvm::Function& function) -> llvm::DominatorTree& {
                return getAnalysis<llvm::DominatorTreeWrapperPass>(function).getDomTree();
            };

            mMemoryModel = CreateFlatMemoryModel(mContext, mSettings, llvmModule, dominators);
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
