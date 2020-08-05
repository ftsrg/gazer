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
#include "gazer/LLVM/Transform/Passes.h"

#include <llvm/Pass.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

namespace
{

class LoopExitCanonizationPass : public llvm::FunctionPass
{
public:
    static char ID;

    LoopExitCanonizationPass()
        : FunctionPass(ID)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override
    {
        au.addRequired<llvm::LoopInfoWrapperPass>();
    }

    bool runOnFunction(llvm::Function& function) override;
};

} // namespace

char LoopExitCanonizationPass::ID;

static void adjustPhiNodes(llvm::BasicBlock* succ, llvm::BasicBlock* oldBB, llvm::BasicBlock* newBB)
{
    auto it = succ->begin();
    while (auto phi = llvm::dyn_cast<llvm::PHINode>(it)) {
        phi->replaceIncomingBlockWith(oldBB, newBB);
        ++it;
    }
}

static bool transformExitsInLoop(llvm::LoopInfo& loopInfo, llvm::Loop* loop)
{
    llvm::Loop* parent = loop->getParentLoop();
    if (parent == nullptr) {
        // This is a top-level loop, all possibly descendants are outside
        return false;
    }

    llvm::SmallVector<llvm::BasicBlock*, 4> exitingBlocks;
    loop->getExitingBlocks(exitingBlocks);

    if (exitingBlocks.empty()) {
        return false;
    }

    for (llvm::BasicBlock* bb : exitingBlocks) {
        // TODO: Switch instruction terminator
        auto br = llvm::dyn_cast<llvm::BranchInst>(bb->getTerminator());
        if (br == nullptr) {
            continue;
        }

        for (unsigned i = 0; i < br->getNumSuccessors(); ++i) {
            auto succ = br->getSuccessor(i);

            if (!loop->contains(succ) && !parent->contains(succ)) {
                // We have a successor outside of the loop and parent loop. Create a new block
                // for the parent loop which will serve as the destination for this jump.
                auto newBB = llvm::BasicBlock::Create(bb->getContext(), "loop_exit", bb->getParent());
                llvm::BranchInst::Create(succ, newBB);

                br->setSuccessor(i, newBB);
                adjustPhiNodes(succ, bb, newBB);
                parent->addBasicBlockToLoop(newBB, loopInfo);
            }
        }
    }

    return true;
}

bool LoopExitCanonizationPass::runOnFunction(llvm::Function& function)
{
    llvm::LoopInfo& loopInfo = getAnalysis<llvm::LoopInfoWrapperPass>().getLoopInfo();

    auto loopsInPostorder = loopInfo.getLoopsInPreorder();
    llvm::reverse(loopsInPostorder);

    bool changed = false;
    for (llvm::Loop* loop : loopsInPostorder) {
        changed |= transformExitsInLoop(loopInfo, loop);
    }

    return changed;
}

llvm::Pass* gazer::createCanonizeLoopExitsPass() {
    return new LoopExitCanonizationPass();
}