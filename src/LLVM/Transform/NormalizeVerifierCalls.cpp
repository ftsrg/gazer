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
///
/// \file This pass normalizes some known verifier calls into a uniform format.
///
//===----------------------------------------------------------------------===//

#include "gazer/LLVM/Transform/Passes.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IRBuilder.h>

using namespace gazer;

namespace
{

// The implementation of this class is partly based on the PromoteVerifierCalls
// pass found in SeaHorn.
// FIXME: We should update the call graph information as we go.
class NormalizeVerifierCallsPass : public llvm::ModulePass
{
public:
    static char ID;

    NormalizeVerifierCallsPass()
        : ModulePass(ID)
    {}

    bool runOnModule(llvm::Module& module) override;
    void runOnFunction(llvm::Function& function);

private:
    llvm::FunctionCallee mAssume;
    llvm::FunctionCallee mError;
};

} // end anonymous namespace

bool NormalizeVerifierCallsPass::runOnModule(llvm::Module& module)
{
    // Insert our uniform functions.
    mAssume = module.getOrInsertFunction(
        "verifier.assume",
        llvm::Type::getVoidTy(module.getContext()),
        llvm::Type::getInt1Ty(module.getContext())
    );

    llvm::AttrBuilder attrBuilder;
    attrBuilder.addAttribute(llvm::Attribute::NoReturn);

    mError = module.getOrInsertFunction(
        "verifier.error",
        llvm::AttributeList::get(
            module.getContext(), llvm::AttributeList::FunctionIndex, attrBuilder
        ),
        llvm::Type::getVoidTy(module.getContext())
    );

    for (llvm::Function& function : module) {
        this->runOnFunction(function);
    }

    return true;
}

char NormalizeVerifierCallsPass::ID;

void NormalizeVerifierCallsPass::runOnFunction(llvm::Function& function)
{
    llvm::SmallVector<llvm::Instruction*, 16> toKill;
    for (llvm::Instruction& inst : llvm::instructions(function)) {
        if (!llvm::isa<llvm::CallInst>(&inst)) {
            continue;
        }

        llvm::CallSite cs(llvm::cast<llvm::CallInst>(&inst));

        llvm::IRBuilder<> builder(function.getContext());
        builder.SetInsertPoint(&inst);

        llvm::Function* callee = cs.getCalledFunction();
        if (callee == nullptr) {
            // TODO: This should be handled for simple cases.
            continue;
        }

        if (callee->getName() == "__VERIFIER_assume" ||
            callee->getName() == "klee_assume" ||
            callee->getName() == "__llbmc_assume"
        ) {
            llvm::Value* condition = cs.getArgument(0);

            // These functions may have differing input argument types, such
            // as i1, i32 or i64. Strip possible ZExt casts and convert to
            // boolean if needed.
            if (auto zext = llvm::dyn_cast<llvm::ZExtInst>(condition)) {
                condition = zext->getOperand(0);
            }

            if (!condition->getType()->isIntegerTy(1)) {
                condition = builder.CreateICmpNE(condition, llvm::ConstantInt::get(
                    condition->getType(), 0
                ));
            }

            builder.CreateCall(mAssume.getCallee(), { condition });
            toKill.emplace_back(&inst);
        }
    }

    for (llvm::Instruction* inst : toKill) {
        inst->dropAllReferences();
        inst->eraseFromParent();
    }
}

llvm::Pass* gazer::createNormalizeVerifierCallsPass()
{
    return new NormalizeVerifierCallsPass();
}
