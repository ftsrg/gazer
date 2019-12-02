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
#ifndef GAZER_LLVM_TRANSFORMS_BACKWARDSLICER_H
#define GAZER_LLVM_TRANSFORMS_BACKWARDSLICER_H

#include "gazer/LLVM/Analysis/PDG.h"

#include <llvm/ADT/SmallVector.h>

#include <functional>

namespace gazer
{

class BackwardSlicer
{
public:
    BackwardSlicer(
        llvm::Function& function,
        std::function<bool(llvm::Instruction*)> criterion
    );

    /// Slices the given function according to the given criteria.
    bool slice();

private:

    bool collectRequiredNodes(llvm::DenseSet<llvm::Instruction*>& visited);
    void sliceBlocks(
        const llvm::DenseSet<llvm::Instruction*>& required,
        llvm::SmallVectorImpl<llvm::BasicBlock*>& preservedBlocks
    );
    void sliceInstructions(llvm::BasicBlock& bb, const llvm::DenseSet<llvm::Instruction*>& required);

private:
    llvm::Function& mFunction;
    std::function<bool(llvm::Instruction*)> mCriterion;
    std::unique_ptr<ProgramDependenceGraph> mPDG;
};

llvm::Pass* createBackwardSlicerPass(std::function<bool(llvm::Instruction*)> criteria);

} // end namespace gazer

#endif