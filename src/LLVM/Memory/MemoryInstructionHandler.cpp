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
#include "gazer/LLVM/Memory/MemoryInstructionHandler.h"
#include "gazer/LLVM/Memory/MemorySSA.h"
#include "gazer/LLVM/Memory/MemoryObject.h"

#include <llvm/Analysis/LoopInfo.h>

using namespace gazer;

static bool hasUsesOutsideOfBlockRange(
    const MemoryObjectDef* def, llvm::ArrayRef<llvm::BasicBlock*> blocks)
{
    // Currently the memory object interface does not support querying the uses of a particular def.
    // What we do instead is we query all uses of the underlying abstract memory object, and check
    // whether their reaching definition is 'def'.
    for (MemoryObjectUse& use : def->getObject()->uses()) {
        if (use.getReachingDef() != def) {
            continue;
        }

        llvm::BasicBlock* bb = use.getInstruction()->getParent();
        if (std::find(blocks.begin(), blocks.end(), bb) == blocks.end()) {
            return true;
        }
    }

    return false;
}

void MemorySSABasedInstructionHandler::declareFunctionVariables(
    llvm2cfa::VariableDeclExtensionPoint& ep)
{
    for (MemoryObject& object : mMemorySSA.objects()) {
        for (MemoryObjectDef& def : object.defs()) {
            if (auto liveOnEntry = llvm::dyn_cast<memory::LiveOnEntryDef>(&def)) {
                if (ep.isEntryProcedure()) {
                    ep.createLocal(&def, object.getObjectType(), "_mem");
                } else {
                    ep.createInput(&def, object.getObjectType(), "_mem");
                }
            } else {
                ep.createLocal(&def, object.getObjectType(), "_mem");
            }
        }

        llvm::TinyPtrVector<memory::RetUse*> retUses;
        for (auto& u : object.uses()) {
            if (auto retUse = llvm::dyn_cast<memory::RetUse>(&u)) {
                retUses.push_back(retUse);
            }
        }

        assert(retUses.empty() || retUses.size() == 1);
        if (!retUses.empty()) {
            Variable* output = ep.getVariableFor(retUses[0]->getReachingDef());
            ep.markOutput(retUses[0]->getReachingDef(), output);
        }
    }
}

void MemorySSABasedInstructionHandler::declareLoopProcedureVariables(
    llvm::Loop* loop, llvm2cfa::LoopVarDeclExtensionPoint& ep)
{
    llvm::ArrayRef<llvm::BasicBlock*> loopBlocks;

    for (MemoryObject& object : mMemorySSA.objects()) {
        for (MemoryObjectDef& def : object.defs()) {
            llvm::BasicBlock* bb = def.getParentBlock();

            Variable* memVar;
            if (bb == loop->getHeader() && def.getKind() == MemoryObjectDef::PHI) {
                memVar = ep.createPhiInput(&def, def.getObject()->getObjectType());
            } else {
                memVar = ep.createLocal(&def, def.getObject()->getObjectType());
            }

            // If the definition has uses outside of the loop it should be marked as output
            if (hasUsesOutsideOfBlockRange(&def, loopBlocks)) {
                ep.createLoopOutput(&def, memVar);
            }
        }

        for (MemoryObjectUse& use : object.uses()) {
            // If we have a use with its reaching def outisde of the loop, it is an input
            llvm::BasicBlock* bb = use.getReachingDef()->getParentBlock();
            if (std::find(loopBlocks.begin(), loopBlocks.end(), bb) == loopBlocks.end()) {
                ep.createInput(use.getReachingDef(), use.getObject()->getObjectType());
            }
        }
    }
}

void MemorySSABasedInstructionHandler::handleBasicBlockEdge(
    const llvm::BasicBlock& source,
    const llvm::BasicBlock& target,
    llvm2cfa::GenerationStepExtensionPoint& ep)
{

}