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
/// \file This file defines the LiftErrorsPass, which lifts error calls from
/// loops and subroutines into the main module. The algorithm is based on the
/// transformation presented in the paper:
///     Akash Lal and Shaz Qadeer.
///     A program transformation for faster goal-directed search.
///     DOI: https://doi.org/10.1109/FMCAD.2014.6987607
///
//===----------------------------------------------------------------------===//
#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Instrumentation/Check.h"
#include "gazer/ADT/Iterator.h"

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

        llvm::BasicBlock* failCopyEntry = nullptr;
        std::vector<llvm::PHINode*> failCopyArgumentPHIs;

        bool canFail() const
        {
            return !selfFails.empty() || !mayFailCalls.empty();
        }
    };

    LiftErrorCalls(llvm::Module& llvmModule, llvm::CallGraph& cg, llvm::Function& entryFunction)
        : mModule(llvmModule), mCallGraph(cg), mEntryFunction(&entryFunction), mBuilder(llvmModule.getContext())
    {}

    bool run();

private:
    void combineErrorsInFunction(llvm::Function* function, FunctionInfo& info);
    void collectFailingCalls(const llvm::CallGraphNode* cgNode);
    void replaceReturnsWithUnreachable(const llvm::SmallVectorImpl<llvm::ReturnInst*>& returns);
    void representArgumentsAsPhiNodes(llvm::Function* function, llvm::BasicBlock* failEntry, FunctionInfo& info, ValueToValueMapTy& vmap);
    void createBranchToFail(const CallSite& call);
    
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
    {
        return CheckRegistry::GetErrorFunction(mModule).getCallee();
    }

    llvm::Type* getErrorCodeType()
    {
        return CheckRegistry::GetErrorCodeType(mModule.getContext());
    }

private:
    llvm::Module& mModule;
    llvm::CallGraph& mCallGraph;
    llvm::Function* mEntryFunction;
    llvm::IRBuilder<> mBuilder;
    llvm::DenseMap<llvm::Function*, FunctionInfo> mInfos;
};

class LiftErrorCallsPass : public llvm::ModulePass
{
public:
    static char ID;

    explicit LiftErrorCallsPass(llvm::Function* function)
        : ModulePass(ID), mEntryFunction(function)
    {}

    void getAnalysisUsage(AnalysisUsage& au) const override
    {
        au.addRequired<CallGraphWrapperPass>();
    }

    bool runOnModule(Module& llvmModule) override
    {
        auto& cg = getAnalysis<CallGraphWrapperPass>();

        LiftErrorCalls impl(llvmModule, cg.getCallGraph(), *mEntryFunction);
        return impl.run();
    }

    llvm::StringRef getPassName() const override
    {
        return "Lift error calls into main.";
    }

private:
    llvm::Function* mEntryFunction;
};

} // end anonymous namespace

char LiftErrorCallsPass::ID;

void LiftErrorCalls::combineErrorsInFunction(llvm::Function* function, FunctionInfo& info)
{
    auto* errorBlock = llvm::BasicBlock::Create(function->getContext(), "error", function);
    mBuilder.SetInsertPoint(errorBlock);

    auto* phi = mBuilder.CreatePHI(getErrorCodeType(), info.selfFails.size(), "error_phi");
    auto* err = mBuilder.CreateCall(getErrorFunction(), {phi});
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

void LiftErrorCalls::collectFailingCalls(const llvm::CallGraphNode* cgNode)
{
    llvm::Function* function = cgNode->getFunction();
    if (function == nullptr || function->isDeclaration()) {
        return;
    }

    // Find error calls in this function.
    llvm::copy_if(
        classof_range<CallInst>(llvm::make_pointer_range(llvm::instructions(function))),
        std::back_inserter(mInfos[function].selfFails),
        [](const llvm::CallInst* call) {
          const llvm::Function* callee = call->getCalledFunction();
          return callee != nullptr && callee->getName() == CheckRegistry::ErrorFunctionName;
        }
    );

    // Find possibly failing calls to other infos.
    for (const auto& [callInst, calleeNode] : *cgNode) {
        llvm::Function* callee = calleeNode->getFunction();
        if (callee == nullptr) {
            // TODO: Indirect function calls are unsupported at the moment.
            continue;
        }

        if (callee->isDeclaration() || !mInfos[callee].canFail()) {
            continue;
        }

        if (auto* call = llvm::dyn_cast<CallInst>(callInst)) {
            mInfos[function].mayFailCalls.emplace_back(call);
        }
    }
}

void LiftErrorCalls::replaceReturnsWithUnreachable(const llvm::SmallVectorImpl<llvm::ReturnInst*>& returns)
{
    for (llvm::ReturnInst* ret : returns) {
        llvm::BasicBlock* retBB = ret->getParent();
        ret->dropAllReferences();
        ret->eraseFromParent();

        mBuilder.SetInsertPoint(retBB);
        mBuilder.CreateCall(getDummyVoidFunc());
        mBuilder.CreateUnreachable();
    }
}

bool LiftErrorCalls::run()
{
    auto sccIt = llvm::scc_begin(&mCallGraph);

    while (!sccIt.isAtEnd()) {
        const std::vector<CallGraphNode*>& scc = *sccIt;
        for (const CallGraphNode* cgNode : scc) {
            this->collectFailingCalls(cgNode);
        }
        ++sccIt;
    }

    if (!mInfos[mEntryFunction].canFail()) {
        // The entry procedure cannot fail, thus the whole program is safe by definition.
        // TODO: This should be changed if we ever start doing modular verification.
        return false;
    }

    // Transform intraprocedurally: create a single, unique error call for
    // each function that may fail. Previous error calls will point to
    // this single error location afterwards.
    for (auto& [function, info] : mInfos) {
        if (info.canFail()) {
            this->combineErrorsInFunction(function, info);
        }
    }

    // Do the interprocedural transformation.
    std::vector<llvm::CallSite> mayFailCallsInMain = mInfos[mEntryFunction].mayFailCalls;

    // Copy the bodies of the possibly-failing functions into main.
    for (auto& [function, info] : mInfos) {
        if (function == mEntryFunction || !info.canFail()) {
            continue;
        }

        llvm::BasicBlock* failEntry = llvm::BasicBlock::Create(
            function->getContext(), function->getName() + "_fail", mEntryFunction
        );
        info.failCopyEntry = failEntry;

        // Create PHI nodes to represent arguments
        llvm::ValueToValueMapTy vmap;
        this->representArgumentsAsPhiNodes(function, failEntry, info, vmap);

        llvm::SmallVector<ReturnInst*, 4> returns;
        llvm::CloneFunctionInto(mEntryFunction, function, vmap, true, returns);

        // The cloned function is in an invalid state, as we do not want the return instructions.
        // Replace all returns with unreachable instructions.
        this->replaceReturnsWithUnreachable(returns);

        // Wire the clone of the unique error call to main's error call.
        auto* mappedErrorCall = dyn_cast_or_null<CallInst>(vmap[info.uniqueErrorCall]);
        assert(mappedErrorCall != nullptr
            && "The error call must map to a call instruction in the cloned function!");

        llvm::BasicBlock* mappedErrorBlock = mappedErrorCall->getParent();
        llvm::Value* errorCodeOperand = mappedErrorCall->getArgOperand(0);
        llvm::BasicBlock* errorBlockInMain = mInfos[mEntryFunction].uniqueErrorCall->getParent();

        llvm::ReplaceInstWithInst(mappedErrorBlock->getTerminator(), BranchInst::Create(errorBlockInMain));
        mInfos[mEntryFunction].uniqueErrorPhi->addIncoming(errorCodeOperand, mappedErrorBlock);

        mappedErrorCall->dropAllReferences();
        mappedErrorCall->eraseFromParent();

        // Add all possible calls into main
        mayFailCallsInMain.reserve(mayFailCallsInMain.size() + info.mayFailCalls.size());
        for (auto cs : info.mayFailCalls) {
            mayFailCallsInMain.emplace_back(vmap[cs.getInstruction()]);
        }

        // Remove the error call from the original function
        info.uniqueErrorCall->dropAllReferences();
        info.uniqueErrorCall->eraseFromParent();
        info.uniqueErrorPhi->dropAllReferences();
        info.uniqueErrorPhi->eraseFromParent();

        auto* clonedEntry = cast<llvm::BasicBlock>(vmap[&function->getEntryBlock()]);
        mBuilder.SetInsertPoint(failEntry);
        mBuilder.CreateBr(clonedEntry);
    }

    for (llvm::CallSite call : mayFailCallsInMain) {
        this->createBranchToFail(call);
    }

    return true;
}
void LiftErrorCalls::createBranchToFail(const CallSite& call)
{
    assert(call.getCalledFunction() != nullptr);
    auto& calleeInfo = mInfos[call.getCalledFunction()];

    // Split the block for each call, create a nondet branch.
    BasicBlock* origBlock = call->getParent();
    BasicBlock* successBlock = SplitBlock(origBlock, call.getInstruction());
    BasicBlock* errorBlock = BasicBlock::Create(mModule.getContext(), "", mEntryFunction);

    // Create the nondetermistic branch between the success and error.
    origBlock->getTerminator()->eraseFromParent();
    mBuilder.SetInsertPoint(origBlock);

    llvm::Instruction* dummyCall = mBuilder.CreateCall(getDummyBoolFunc());
    mBuilder.CreateCondBr(dummyCall, errorBlock, successBlock);

    // Branch to the appropriate basic block
    mBuilder.SetInsertPoint(errorBlock);
    mBuilder.CreateBr(calleeInfo.failCopyEntry);

    for (unsigned i = 0; i < call.arg_size(); ++i) {
        calleeInfo.failCopyArgumentPHIs[i]->addIncoming(call.getArgOperand(i), errorBlock);
    }
}

void LiftErrorCalls::representArgumentsAsPhiNodes(
    Function* function,
    BasicBlock* failEntry,
    LiftErrorCalls::FunctionInfo& info,
    ValueToValueMapTy& vmap)
{
    info.failCopyArgumentPHIs.resize(function->arg_size());
    mBuilder.SetInsertPoint(failEntry);
    for (size_t i = 0; i < function->arg_size(); ++i) {
        Argument& argument = *(function->arg_begin() + i);
        PHINode* phi = mBuilder.CreatePHI(argument.getType(), 0, "");
        info.failCopyArgumentPHIs[i] = phi;
        vmap[&argument] = phi;
    }
}

llvm::Pass* gazer::createLiftErrorCallsPass(llvm::Function& entry)
{
    return new LiftErrorCallsPass(&entry);
}
