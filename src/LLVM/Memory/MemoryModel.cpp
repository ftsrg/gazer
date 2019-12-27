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

using namespace gazer;

using gazer::memory::MemorySSA;
using gazer::memory::MemorySSABuilder;

void MemoryModel::initialize(llvm::Module& module)
{
    for (llvm::Function& function : module) {
        if (function.isDeclaration()) {
            continue;
        }

        llvm::DominatorTree dominatorTree(function);
        MemorySSABuilder builder(function, mDataLayout, dominatorTree);
        this->initializeFunction(function, builder);

        auto memSSA = builder.build();
        mFunctions.try_emplace(&function, std::move(memSSA));
    }
}

ExprPtr MemoryModel::handleLiveOnEntry(memory::LiveOnEntryDef* def, llvm2cfa::GenerationStepExtensionPoint& ep)
{
    return nullptr;
}

void CollectMemoryDefsUsesVisitor::visitStoreInst(llvm::StoreInst& store)
{
    llvm::Value* val = store.getValueOperand();
    if (val == *U) {
        PI.setEscapedAndAborted(&store);
    }

    mBuilder.createStoreDef(mObject, store);
}

void CollectMemoryDefsUsesVisitor::visitCallInst(llvm::CallInst& call)
{
    InstVisitor::visitCallInst(call);
}

void CollectMemoryDefsUsesVisitor::visitPHINode(llvm::PHINode& phi)
{
    InstVisitor::visitPHINode(phi);
}

void CollectMemoryDefsUsesVisitor::visitSelectInst(llvm::SelectInst& select)
{
    InstVisitor::visitSelectInst(select);
}

void CollectMemoryDefsUsesVisitor::visitInstruction(llvm::Instruction& inst)
{
    InstVisitor::visitInstruction(inst);
}
