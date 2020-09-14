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
#ifndef GAZER_LLVM_TRANSFORMS_ALLOCASTOREPASS_H
#define GAZER_LLVM_TRANSFORMS_ALLOCASTOREPASS_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "gazer/LLVM/Analysis/PDG.h"

#include <llvm/ADT/SmallVector.h>

namespace gazer
{

// Inserts a store of an undef value after each alloca instructions
// (This prevents some incorrect false values due to uninitialized variables,
// but the UndefToNondet Pass has to be run after this one immediately)
class AllocaStorePass : public llvm::FunctionPass
{
public:
    static char ID;
    AllocaStorePass() : FunctionPass(ID) {}

    bool runOnFunction(llvm::Function &F) override {
        llvm::IRBuilder<> builder{F.getContext()};
        // iterate over the basic blocks of a function
        for (auto &BB : F) {
            // iterate over the instructions of a basic block
            for (auto &I : BB) {
                if (llvm::AllocaInst *CI = llvm::dyn_cast<llvm::AllocaInst>(&I)) {
                    builder.SetInsertPoint(CI->getNextNode());
                    builder.CreateAlignedStore(llvm::UndefValue::get(CI->getAllocatedType()), CI, 4); // TODO don't hardcode alignment               
                }
            }
        }                        
                                
        return false;
    }
};
char AllocaStorePass::ID;

llvm::Pass* createAllocaStorePass() { return new AllocaStorePass(); }

}

#endif
