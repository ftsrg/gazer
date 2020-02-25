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

    static llvm::Function* shouldInlineGlobal(llvm::CallGraph& cg, llvm::GlobalVariable& gv);
};

llvm::Function* InlineGlobalVariablesPass::shouldInlineGlobal(llvm::CallGraph& cg, llvm::GlobalVariable& gv)
{
    llvm::GlobalStatus status;

    if (llvm::GlobalStatus::analyzeGlobal(&gv, status)) {
        return nullptr;
    }

    if (status.HasMultipleAccessingFunctions
        || status.AccessingFunction == nullptr
    ) {
        return nullptr;
    }

    llvm::CallGraphNode* cgNode = cg[status.AccessingFunction];
    if (isRecursive(cgNode)) {
        return nullptr;
    }

    // Check if we can transform constant users into instructions.
    // Based on the similar check found in SeaHorn
    for (llvm::User* user : gv.users()) {
        if (isa<llvm::Instruction>(user)) {
            continue;
        }

        if (!isa<llvm::ConstantExpr>(user)) {
            // Non instruction, non-constantexpr user; cannot convert this.
            return nullptr;
        }

        for (llvm::User* uu : user->users()) {
            if (!isa<llvm::Instruction>(uu)) {
                // A ConstantExpr used by another constant - we do not want
                // recurse further, so we just return false.
                return nullptr;
            }
        }
    }

    // GlobalStatus::AccessingFunction is somewhy const, however we will have to modify the original function.
    // This should not cause problems as we will not depend on the GlobalStatus object any further.
    return const_cast<llvm::Function*>(status.AccessingFunction);
}

char InlineGlobalVariablesPass::ID = 0;

void transformConstantUsersToInstructions(llvm::Constant& constant)
{
    // Based on the similar utility found in SeaHorn
    llvm::SmallVector<llvm::ConstantExpr*, 4> ceUsers;
    for (llvm::User* user : constant.users()) {
        if (auto ce = llvm::dyn_cast<llvm::ConstantExpr>(user)) {
            ceUsers.emplace_back(ce);
        }
    }

    for (llvm::ConstantExpr* user : ceUsers) {
        llvm::SmallVector<llvm::User*, 4> usersOfUser;
        for (llvm::User* uu : user->users()) {
            usersOfUser.emplace_back(uu);
        }

        for (llvm::User* uu : usersOfUser) {
            auto ui = llvm::cast<llvm::Instruction>(uu); 
            auto newUser = user->getAsInstruction();
            newUser->insertBefore(ui);
            ui->replaceUsesOfWith(user, newUser);
        }
        user->dropAllReferences();
    }
}

bool InlineGlobalVariablesPass::runOnModule(Module& module)
{
    if (module.global_begin() == module.global_end()) {
        // No globals to inline
        return false;
    }

    llvm::CallGraph& cg = getAnalysis<llvm::CallGraphWrapperPass>().getCallGraph();

    // Create a dbg declaration if it does not exist yet.
    Intrinsic::getDeclaration(&module, Intrinsic::dbg_declare);

    IRBuilder<> builder(module.getContext());

    DIBuilder diBuilder(module);

    auto gvIt = module.global_begin();
    while (gvIt != module.global_end()) {
        GlobalVariable& gv = *(gvIt++);
        auto type = gv.getType()->getElementType();

        if (!gv.hasInitializer()) {
            continue;
        }

        llvm::Function* target = this->shouldInlineGlobal(cg, gv);
        if (target == nullptr) {
            continue;
        }

        builder.SetInsertPoint(&target->getEntryBlock(), target->getEntryBlock().begin());
        AllocaInst* alloc = builder.CreateAlloca(type, nullptr, gv.getName());

        Constant* init = gv.getInitializer();
        builder.CreateAlignedStore(init, alloc, module.getDataLayout().getABITypeAlignment(type));

        // TODO: We should check external calls and clobber the alloca with a nondetermistic
        // store if the ExternFuncGlobalBehavior setting requires this.

        // Add some metadata stuff
        auto mark = GazerIntrinsic::GetOrInsertInlinedGlobalWrite(module, gv.getType()->getPointerElementType());
        cg.getOrInsertFunction(llvm::cast<llvm::Function>(mark.getCallee()));

        // FIXME: There should be a more intelligent way for finding the DIGlobalVariable
        llvm::SmallVector<std::pair<unsigned, MDNode*>, 2> md;
        gv.getAllMetadata(md);

        llvm::DIGlobalVariableExpression* diGlobalExpr = nullptr;
        std::for_each(md.begin(), md.end(), [&diGlobalExpr](auto pair) {
            if (auto ge = dyn_cast<DIGlobalVariableExpression>(pair.second)) {
                diGlobalExpr = ge;
            }
        });

        if (diGlobalExpr != nullptr) {
            auto diGlobalVariable = diGlobalExpr->getVariable();
            for (llvm::Value* user : gv.users()) {
                if (auto inst = llvm::dyn_cast<StoreInst>(user)) {
                    llvm::Value* value = inst->getOperand(0);

                    CallInst* call = CallInst::Create(
                        mark.getFunctionType(), mark.getCallee(), {
                            value, MetadataAsValue::get(module.getContext(), diGlobalVariable)
                        }
                    );

                    call->setDebugLoc(inst->getDebugLoc());
                    call->insertAfter(inst);
                }
            }
        }

        transformConstantUsersToInstructions(gv);
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