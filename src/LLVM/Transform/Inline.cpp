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
//
/// \file This file implements our own inlining pass, which is more restricted,
/// but faster than the inlining utilities found in LLVM.
//
//===----------------------------------------------------------------------===//

#include "TransformUtils.h"

#include "gazer/LLVM/Transform/Passes.h"

#include <llvm/Pass.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Transforms/Utils/Cloning.h>

using namespace gazer;

namespace
{

class InlinePass : public llvm::ModulePass
{
public:
    static char ID;

public:
    InlinePass(llvm::Function* entry)
        : ModulePass(ID), mEntryFunction(entry)
    {
        assert(mEntryFunction != nullptr);
    }

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override
    {
        au.addRequired<llvm::CallGraphWrapperPass>();
    }

    bool runOnModule(llvm::Module& module) override;

    llvm::StringRef getPassName() const override {
        return "Simplified inling";
    }

private:
    bool shouldInlineFunction(llvm::CallGraphNode* target, unsigned allowedRefs);

    llvm::Function* mEntryFunction;
};

} // end anonymous namespace

char InlinePass::ID;

bool InlinePass::shouldInlineFunction(llvm::CallGraphNode* target, unsigned allowedRefs)
{
    // We only want to inline functions which are non-recursive,
    // used only once and do not have variable argument lists.
    return target->getNumReferences() <= allowedRefs
        && !isRecursive(target)
        && !target->getFunction()->isVarArg();
}

bool InlinePass::runOnModule(llvm::Module& module)
{
    bool changed = false;
    llvm::CallGraph& cg = getAnalysis<llvm::CallGraphWrapperPass>().getCallGraph();

    llvm::InlineFunctionInfo ifi(&cg);
    llvm::SmallVector<llvm::CallSite, 16> wl;

    llvm::CallGraphNode* entryCG = cg[mEntryFunction];

    for (auto& [call, target] : *entryCG) {
        if (this->shouldInlineFunction(target, 1)) {
            wl.emplace_back(call);
        }
    }

    while (!wl.empty()) {
        llvm::CallSite cs = wl.pop_back_val();
        bool success = llvm::InlineFunction(cs, ifi);
        changed |= success;

        for (llvm::Value* newCall : ifi.InlinedCalls) {
            llvm::CallSite newCS(newCall);
            auto callee = newCS.getCalledFunction();
            if (callee == nullptr) {
                continue;
            }

            llvm::CallGraphNode* calleeNode = cg[callee];
            if (this->shouldInlineFunction(calleeNode, 2)) {
                wl.emplace_back(newCS);
            }
        }
    }

    return changed;
}

llvm::Pass* gazer::createSimpleInlinerPass(llvm::Function* entry)
{
    return new InlinePass(entry);
}
