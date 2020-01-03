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

#include "TransformUtils.h"

#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/Transforms/Utils/GlobalStatus.h>

using namespace llvm;
using namespace gazer;

namespace
{

struct InlineGlobalVariablesPass final : public ModulePass
{
    static char ID;

    InlineGlobalVariablesPass()
        : ModulePass(ID)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override
    {
        au.addRequired<llvm::CallGraphWrapperPass>();
    }

    bool runOnModule(Module& module) override;

    bool shouldInlineGlobal(llvm::CallGraph& cg, llvm::GlobalVariable& gv) const;
};

bool InlineGlobalVariablesPass::shouldInlineGlobal(llvm::CallGraph& cg, llvm::GlobalVariable& gv) const
{
    llvm::GlobalStatus status;

    if (llvm::GlobalStatus::analyzeGlobal(&gv, status)) {
        return false;
    }

    if (status.HasMultipleAccessingFunctions
        || status.AccessingFunction == nullptr
    ) {
        return false;
    }

    llvm::CallGraphNode* cgNode = cg[status.AccessingFunction];
    if (isRecursive(cgNode)) {
        return false;
    }

    return true;
}

char InlineGlobalVariablesPass::ID = 0;

bool InlineGlobalVariablesPass::runOnModule(Module& module)
{
    Function* main = module.getFunction("main");
    if (main == nullptr) {
        // No main function found or not all functions were inlined.
        return false;
    }

    if (module.global_begin() == module.global_end()) {
        // No globals to inline
        return false;
    }

    llvm::CallGraph& cg = getAnalysis<llvm::CallGraphWrapperPass>().getCallGraph();

    auto mark = GazerIntrinsic::GetOrInsertInlinedGlobalWrite(module);
    cg.getOrInsertFunction(llvm::cast<llvm::Function>(mark.getCallee()));

    // Create a dbg declaration if it does not exist yet.
    Intrinsic::getDeclaration(&module, Intrinsic::dbg_declare);

    IRBuilder<> builder(module.getContext());
    builder.SetInsertPoint(&main->getEntryBlock(), main->getEntryBlock().begin());

    DIBuilder diBuilder(module);

    auto gvIt = module.global_begin();
    while (gvIt != module.global_end()) {
        GlobalVariable& gv = *(gvIt++);

        if (gv.isConstant()) {
            // Do not inline constants.
            continue;
        }

        if (!this->shouldInlineGlobal(cg, gv)) {
            continue;
        }

        auto type = gv.getType()->getElementType();
        AllocaInst* alloc = builder.CreateAlloca(type, nullptr, gv.getName());

        // TODO: I'm not entirely sure if this is sound - this undef should probably
        // be replaced by a nondetermistic call.
        Value* init = gv.hasInitializer() ? gv.getInitializer() : UndefValue::get(type);
        builder.CreateStore(init, alloc);

        // TODO: We should check external calls and clobber the alloca with a nondetermistic
        // store if the ExternFuncGlobalBehavior setting requires this.

        // Add some metadata stuff
        // FIXME: There should be a more intelligent way for finding
        //  the DIGlobalVariable
        llvm::SmallVector<std::pair<unsigned, MDNode*>, 2> md;
        gv.getAllMetadata(md);

        llvm::DIGlobalVariableExpression* diGlobalExpr = nullptr;
        std::for_each(md.begin(), md.end(), [&diGlobalExpr](auto pair) {
            if (auto ge = dyn_cast<DIGlobalVariableExpression>(pair.second)) {
                diGlobalExpr = ge;
            }
        });

        if (diGlobalExpr) {
            auto diGlobalVariable = diGlobalExpr->getVariable();
            for (llvm::Value* user : gv.users()) {
                if (auto inst = llvm::dyn_cast<StoreInst>(user)) {
                    llvm::Value* value = inst->getOperand(0);
                    CallInst* call = CallInst::Create(
                        mark.getFunctionType(), mark.getCallee(), {
                            MetadataAsValue::get(module.getContext(), ValueAsMetadata::get(value)),
                            MetadataAsValue::get(module.getContext(), diGlobalVariable)
                        }
                    );

                    call->setDebugLoc(inst->getDebugLoc());
                    call->insertAfter(inst);
                }
            }
        }

        gv.replaceAllUsesWith(alloc);
        gv.eraseFromParent();
    }

    return true;
}

} // end anonymous namespace

namespace gazer
{

llvm::Pass* createInlineGlobalVariablesPass() {
    return new InlineGlobalVariablesPass();
}

} // end namespace gazer