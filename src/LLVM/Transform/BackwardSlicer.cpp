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

#include "gazer/LLVM/Transform/BackwardSlicer.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>

using namespace gazer;
using namespace llvm;

BackwardSlicer::BackwardSlicer(
    llvm::Function& function,
    std::function<bool(llvm::Instruction*)> criterion
) : mFunction(function), mCriterion(criterion)
{
    llvm::PostDominatorTree pdt(function);
    mPDG = ProgramDependenceGraph::Create(function, pdt);
}

bool BackwardSlicer::collectRequiredNodes(llvm::DenseSet<llvm::Instruction*>& visited)
{
    llvm::SmallVector<llvm::Instruction*, 32> wl;

    // Insert the original slicing criteria
    for (llvm::Instruction& inst : llvm::instructions(mFunction)) {
        if (mCriterion(&inst)) {
            wl.push_back(&inst);
        }
    }

    if (wl.empty()) {
        // None of the nodes matched the slicing criterion, bail out.
        return false;
    }

    // Walk the PDG, collecting needed nodes
    while (!wl.empty()) {
        Instruction* current = wl.pop_back_val();
        visited.insert(current);

        //llvm::errs() << "Visiting " << *current << "\n";
        auto pdgNode = mPDG->getNode(current);
        for (PDGEdge* edge : pdgNode->incoming()) {
            llvm::Instruction* inst = edge->getSource()->getInstruction();
            //llvm::errs() << "  Child " << *inst << "\n";
            if (visited.count(inst) == 0) {
                //llvm::errs() << "  not visited yet\n";
                wl.push_back(inst);
            }
        }
    }

    return true;
}

void BackwardSlicer::sliceBlocks(
    const llvm::DenseSet<llvm::Instruction*>& required,
    llvm::SmallVectorImpl<llvm::BasicBlock*>& preservedBlocks
) {
    auto& list = mFunction.getBasicBlockList();
    auto it = list.begin(), ie = list.end();

    while (it != ie) {
        llvm::BasicBlock& bb = *(it++);        
        bool shouldRemove = std::all_of(bb.begin(), bb.end(), [&required](llvm::Instruction& inst) {
            return required.count(&inst) == 0;
        });

        if (!shouldRemove) {
            preservedBlocks.push_back(&bb);
            continue;
        }

        // Remove all instructions in this block
        for (auto j = bb.begin(); j != bb.end();) {
            llvm::Instruction& inst = *(j++);

            inst.replaceAllUsesWith(UndefValue::get(inst.getType()));
            inst.dropAllReferences();
            inst.eraseFromParent();
        }

        // Update successors PHI's
        for (llvm::BasicBlock* succ : llvm::successors(&bb)) {
            succ->removePredecessor(&bb);
        }

        // Insert a new 'unreachable' instruction as the terminator
        auto unreachable = new UnreachableInst(mFunction.getContext());
        bb.getInstList().push_back(unreachable);
    }
}

void BackwardSlicer::sliceInstructions(
    llvm::BasicBlock& bb,
    const llvm::DenseSet<llvm::Instruction*>& required
) {
    auto& list = bb.getInstList();
    auto it = list.begin(), ie = list.end();

    while (it != ie) {
        llvm::Instruction& current = *(it++);

        if (required.count(&current) != 0) {
            continue;
        }

        // Do not remove terminator instructions, as they are needed for well-formedness.
        if (&current == bb.getTerminator()) {
            continue;
        }

        // Do not remove intrinsics and debug-related stuff.
        // TODO: We should be more refined here, instead of keeping all calls.
        if (auto call = llvm::dyn_cast<llvm::CallInst>(&current)) {
            continue;
        }

        current.replaceAllUsesWith(UndefValue::get(current.getType()));
        current.dropAllReferences();
        current.eraseFromParent();
    }
}

bool BackwardSlicer::slice()
{
    llvm::DenseSet<llvm::Instruction*> required;
    bool shouldContinue = this->collectRequiredNodes(required);

    //mFunction.dump();
    //mPDG->view();
    if (!shouldContinue) {
        return false;
    }
    
    // Slice blocks which should completely be removed.
    llvm::SmallVector<llvm::BasicBlock*, 32> preservedBlocks;
    this->sliceBlocks(required, preservedBlocks);

    // Slice the remaining blocks.
    for (llvm::BasicBlock* bb : preservedBlocks) {
        this->sliceInstructions(*bb, required);
    }

    //mFunction.dump();

    return true;
}

// LLVM pass implementation
//===----------------------------------------------------------------------===//

namespace
{

class BackwardSlicerPass : public llvm::FunctionPass
{
public:
    char ID;

    BackwardSlicerPass(std::function<bool(llvm::Instruction*)> criteria)
        : FunctionPass(ID), mCriteria(criteria)
    {}

    bool runOnFunction(llvm::Function& function) override
    {
        BackwardSlicer slicer(function, mCriteria);
        return slicer.slice();
    }

private:
    std::function<bool(llvm::Instruction*)> mCriteria;
};

} // end anonymous namespace

llvm::Pass* gazer::createBackwardSlicerPass(std::function<bool(llvm::Instruction*)> criteria)
{
    return new BackwardSlicerPass(criteria);
}