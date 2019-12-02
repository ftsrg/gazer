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
/// \file This file defines the LiftErrorsPass, which lifts error calls
/// from loops and subroutines into the main module.
///
//===----------------------------------------------------------------------===//
#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Instrumentation/Check.h"
#include "gazer/LLVM/Transform/BackwardSlicer.h"

#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SCCIterator.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include <unordered_map>

using namespace gazer;
using namespace llvm;

namespace
{

class LiftErrorCalls
{
public:
    struct FunctionInfo
    {
        std::vector<llvm::CallInst*> selfFails;
        std::vector<llvm::CallSite> mayFailCalls;
        llvm::PHINode* uniqueErrorPhi = nullptr;
        llvm::CallInst* uniqueErrorCall = nullptr;

        llvm::Function* alwaysFailClone = nullptr;
        llvm::BasicBlock* failCopyEntry = nullptr;
        std::vector<llvm::PHINode*> failCopyArgumentPHIs;
        
        llvm::PHINode* failCloneErrorPhi = nullptr;

        bool canFail() const
        { return !selfFails.empty() || !mayFailCalls.empty(); }
    };

public:
    LiftErrorCalls(llvm::Module& module, llvm::CallGraph& cg)
        : mModule(module),
        mCallGraph(cg), mBuilder(module.getContext())
    {}

    bool run();

private:
    void combineErrorsInFunction(llvm::Function* function, FunctionInfo& info);
    
    llvm::FunctionCallee getDummyBoolFunc()
    {
        return mModule.getOrInsertFunction("gazer.dummy_nondet.i1", llvm::FunctionType::get(
            llvm::Type::getInt1Ty(mModule.getContext()), /*isVarArg=*/false
        ));
    }

    llvm::FunctionCallee getDummyVoidFunc()
    {
        return mModule.getOrInsertFunction(
            "gazer.dummy.void",
            llvm::FunctionType::get(llvm::Type::getVoidTy(mModule.getContext()), false)
        );
    }

    llvm::Value* getErrorFunction()
    { return CheckRegistry::GetErrorFunction(mModule).getCallee(); }

    llvm::Type* getErrorCodeType()
    { return CheckRegistry::GetErrorCodeType(mModule.getContext()); }

private:
    llvm::Module& mModule;
    llvm::CallGraph& mCallGraph;
    llvm::IRBuilder<> mBuilder;
    llvm::DenseMap<llvm::Function*, FunctionInfo> mInfos;
};

struct LiftErrorCallsPass : public llvm::ModulePass
{
    static char ID;

    LiftErrorCallsPass()
        : ModulePass(ID)
    {}

    void getAnalysisUsage(AnalysisUsage& au) const override
    {
        au.addRequired<CallGraphWrapperPass>();
    }

    bool runOnModule(Module& module) override
    {
        auto& cg = getAnalysis<CallGraphWrapperPass>();

        LiftErrorCalls impl(module, cg.getCallGraph());
        return impl.run();
    }

    llvm::StringRef getPassName() const override
    { return "Lift error calls into main."; }
};

} // end anonymous namespace

char LiftErrorCallsPass::ID;

void LiftErrorCalls::combineErrorsInFunction(llvm::Function* function, FunctionInfo& info)
{
    auto errorBlock = llvm::BasicBlock::Create(function->getContext(), "error", function);
    mBuilder.SetInsertPoint(errorBlock);

    auto phi = mBuilder.CreatePHI(getErrorCodeType(), info.selfFails.size(), "error_phi");
    auto err = mBuilder.CreateCall(getErrorFunction(), {phi});
    mBuilder.CreateUnreachable();

    for (llvm::CallInst* errorCall : info.selfFails) {
        llvm::Value* code = errorCall->getArgOperand(0);
        phi->addIncoming(code, errorCall->getParent());

        llvm::ReplaceInstWithInst(
            errorCall->getParent()->getTerminator(),
            llvm::BranchInst::Create(errorBlock)
        );

        errorCall->eraseFromParent();
    }

    info.uniqueErrorPhi = phi;
    info.uniqueErrorCall = err;
}

bool LiftErrorCalls::run()
{
    llvm::Function* main = mModule.getFunction("main");
    assert(main != nullptr && "The entry point must exist!");

    auto sccIt = llvm::scc_begin(&mCallGraph);

    while (!sccIt.isAtEnd()) {
        const std::vector<CallGraphNode*>& scc = *sccIt;

        for (auto cgNode : scc) {
            // TODO: What happens if we get an actual SCC?
            auto function = cgNode->getFunction();
            if (function == nullptr || function->isDeclaration()) {
                continue;
            }

            // Find error calls in this function.
            for (auto& inst : llvm::instructions(function)) {
                if (auto call = llvm::dyn_cast<CallInst>(&inst)) {
                    llvm::Function* callee = call->getCalledFunction();
                    if (callee != nullptr && callee->getName() == CheckRegistry::ErrorFunctionName) {
                        mInfos[function].selfFails.push_back(call);
                    }
                }
            }

            // Find possibly failing calls to other infos.
            for (auto& callRecord : *cgNode) {
                llvm::Function* callee = callRecord.second->getFunction();
                if (callee->isDeclaration() || !mInfos[callee].canFail()) {
                    continue;
                }

                if (auto call = llvm::dyn_cast<CallInst>(callRecord.first)) {
                    mInfos[function].mayFailCalls.push_back(call);
                }
            }
        }

        ++sccIt;
    }

    if (!mInfos[main].canFail()) {
        // The entry procedure cannot fail, thus the whole program is safe by definition.
        // TODO: This should be changed if we ever start doing modular verification.
        return false;
    }

    // Transform intraprocedurally: create a single, unique error call for
    // each function that may fail. Previous error calls will point to
    // this single error location afterwards.
    for (auto& [function, info] : mInfos) {
        if (info.canFail()) {
            combineErrorsInFunction(function, info);
        }
    }

    // Do the interprocedural transformation.
    std::vector<llvm::CallSite> mayFailCallsInMain = mInfos[main].mayFailCalls;

    // Copy the bodies of the possibly-failing functions into main.
    for (auto& [function, info] : mInfos) {
        if (function == main || !info.canFail()) {
            continue;
        }

        llvm::BasicBlock* failEntry = llvm::BasicBlock::Create(
            function->getContext(), function->getName() + "_fail", main
        );
        info.failCopyEntry = failEntry;

        // Create PHI nodes to represent arguments
        llvm::ValueToValueMapTy vmap;
        info.failCopyArgumentPHIs.resize(function->arg_size());
        mBuilder.SetInsertPoint(failEntry);
        for (size_t i = 0; i < function->arg_size(); ++i) {
            llvm::Argument& argument = *(function->arg_begin() + i);
            llvm::PHINode* phi = mBuilder.CreatePHI(argument.getType(), 0, "");
            info.failCopyArgumentPHIs[i] = phi;
            vmap[&argument] = phi;
        }

        llvm::SmallVector<ReturnInst*, 4> returns;
        llvm::CloneFunctionInto(main, function, vmap, true, returns);

        // The cloned function is in a invalid state, as we do not want the return instructions.
        // Replace all returns with unreachable's.
        for (auto ret : returns) {
            llvm::BasicBlock* retBB = ret->getParent();
            ret->dropAllReferences();
            ret->eraseFromParent();

            mBuilder.SetInsertPoint(retBB);
            mBuilder.CreateCall(getDummyVoidFunc());
            mBuilder.CreateUnreachable();
        }

        // Wire the clone of the unique error call to main's error call.
        auto mappedErrorCall = dyn_cast_or_null<CallInst>(vmap[info.uniqueErrorCall]);
        assert(mappedErrorCall != nullptr
            && "The error call must map to a call instruction in the cloned function!");

        llvm::BasicBlock* mappedErrorBlock = mappedErrorCall->getParent();
        llvm::Value* errorCodeOperand = mappedErrorCall->getArgOperand(0);
        llvm::BasicBlock* errorBlockInMain = mInfos[main].uniqueErrorCall->getParent();

        llvm::ReplaceInstWithInst(mappedErrorBlock->getTerminator(), BranchInst::Create(errorBlockInMain));
        mInfos[main].uniqueErrorPhi->addIncoming(errorCodeOperand, mappedErrorBlock);

        mappedErrorCall->dropAllReferences();
        mappedErrorCall->eraseFromParent();

        // Add all possible calls into main
        mayFailCallsInMain.reserve(mayFailCallsInMain.size() + info.mayFailCalls.size());
        for (auto cs : info.mayFailCalls) {
            mayFailCallsInMain.push_back(CallSite(vmap[cs.getInstruction()]));
        }

        // Remove the error call from the original function
        info.uniqueErrorCall->dropAllReferences();
        info.uniqueErrorCall->eraseFromParent();
        info.uniqueErrorPhi->dropAllReferences();
        info.uniqueErrorPhi->eraseFromParent();

        BasicBlock* clonedEntry = cast<llvm::BasicBlock>(vmap[&function->getEntryBlock()]);
        mBuilder.SetInsertPoint(failEntry);
        mBuilder.CreateBr(clonedEntry);
    }

    for (llvm::CallSite call : mayFailCallsInMain) {
        assert(call.getCalledFunction() != nullptr);
        auto& calleeInfo = mInfos[call.getCalledFunction()];

        // Split the block for each call, create a nondet branch.
        llvm::BasicBlock* origBlock = call->getParent();
        llvm::BasicBlock* successBlock = llvm::SplitBlock(origBlock, call.getInstruction());
        llvm::BasicBlock* errorBlock = llvm::BasicBlock::Create(mModule.getContext(), "", main);

        // Create the nondetermistic branch between the success and error.
        origBlock->getTerminator()->eraseFromParent();
        mBuilder.SetInsertPoint(origBlock);

        auto dummyCall = mBuilder.CreateCall(getDummyBoolFunc());
        mBuilder.CreateCondBr(dummyCall, errorBlock, successBlock);

        // Branch to the appropriate basic block
        mBuilder.SetInsertPoint(errorBlock);
        mBuilder.CreateBr(calleeInfo.failCopyEntry);

        for (size_t i = 0; i < call.arg_size(); ++i) {
            calleeInfo.failCopyArgumentPHIs[i]->addIncoming(call.getArgOperand(i), errorBlock);
        }
    }

    return true;
}

llvm::Pass* gazer::createLiftErrorCallsPass()
{
    return new LiftErrorCallsPass();
}
