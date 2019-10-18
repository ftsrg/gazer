/// \file This file defines the LiftErrorsPass, which lifts error calls
/// from loops and subroutines into the main module.
#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Instrumentation/Check.h"

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
        std::vector<llvm::CallInst*> mayFailCalls;
        llvm::PHINode* uniqueErrorPhi = nullptr;
        llvm::CallInst* uniqueErrorCall = nullptr;

        llvm::Function* alwaysFailClone = nullptr;
        llvm::PHINode* failCloneErrorPhi = nullptr;
        std::vector<llvm::CallInst*> mayFailCallsInClone;

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
    void splitMayFailCallsInFunction(llvm::Function* function, llvm::PHINode* errorPhi, std::vector<llvm::CallInst*>& calls);
    llvm::FunctionCallee getDummyBoolFunc() {
        return mModule.getOrInsertFunction("gazer.dummy_nondet.i1", llvm::FunctionType::get(
            llvm::Type::getInt1Ty(mModule.getContext()), /*isVarArg=*/false
        ));
    }

    llvm::FunctionCallee getDummyVoidFunc() {
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

void LiftErrorCalls::splitMayFailCallsInFunction(
    llvm::Function* function,
    llvm::PHINode* errorPhi,
    std::vector<llvm::CallInst*>& calls
) {
    for (llvm::CallInst* call : calls) {
        // Split the block for each call, create a nondet branch.
        llvm::BasicBlock* origBlock = call->getParent();
        llvm::BasicBlock* successBlock = llvm::SplitBlock(origBlock, call);
        llvm::BasicBlock* errorBlock = llvm::BasicBlock::Create(mModule.getContext(), "", function);

        // Create the nondetermistic branch between the success and error.
        origBlock->getTerminator()->eraseFromParent();
        mBuilder.SetInsertPoint(origBlock);

        auto dummyCall = mBuilder.CreateCall(getDummyBoolFunc());
        mBuilder.CreateCondBr(dummyCall, errorBlock, successBlock);

        // Add the failing call to 'errorBlock'.
        auto clone = mInfos[call->getCalledFunction()].alwaysFailClone;
        assert(clone != nullptr && "A failing function must have a failing clone!");
        std::vector<llvm::Value*> callArgs(call->arg_begin(), call->arg_end());

        mBuilder.SetInsertPoint(errorBlock);
        auto errorCall = mBuilder.CreateCall(llvm::FunctionCallee(clone), callArgs);

        // Terminate the error call block with a branch to the unique error block.
        // Add the returned error code to the PHI node.
        mBuilder.CreateBr(errorPhi->getParent());
        errorPhi->addIncoming(errorCall, errorBlock);
    }
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
    for (auto& entry : mInfos) {
        if (entry.second.canFail()) {
            combineErrorsInFunction(entry.first, entry.second);
        }
    }

    // Do the interprocedural transformation.
    for (auto& entry : mInfos) {
        llvm::Function* function = entry.first;
        auto& info = entry.second;

        if (function == main || !info.canFail()) {
            continue;
        }

        // We will have two versions of each function: one that always fails, and one that cannot fail.
        // The failing clone will have the same operand list as the original, but the return type shall
        // be the error code.
        auto failFuncTy = llvm::FunctionType::get(
            getErrorCodeType(), function->getFunctionType()->params(), function->getFunctionType()->isVarArg()
        );

        auto clone = llvm::Function::Create(
            failFuncTy, function->getLinkage(), function->getName() + "_fail", &mModule
        );

        // Perform the cloning.
        llvm::ValueToValueMapTy vmap;
        for (size_t i = 0; i < function->arg_size(); ++i) {
            llvm::Argument& argument = *(function->arg_begin() + i);
            vmap[&argument] = clone->arg_begin() + i;
        }

        llvm::SmallVector<ReturnInst*, 4> returns;
        llvm::CloneFunctionInto(clone, function, vmap, true, returns);

        // The cloned function is in a invalid state, as the return type will probably differ from the original.
        // Replace all returns with unreachable's.
        for (auto ret : returns) {
            llvm::BasicBlock* retBB = ret->getParent();
            ret->dropAllReferences();
            ret->eraseFromParent();

            mBuilder.SetInsertPoint(retBB);
            mBuilder.CreateCall(getDummyVoidFunc());
            mBuilder.CreateUnreachable();
        }

        // Replace the error call in the unique error block with a return instruction.
        auto mappedErrorCall = dyn_cast_or_null<CallInst>(vmap[info.uniqueErrorCall]);
        assert(mappedErrorCall != nullptr && "The error call must map to a call instruction in the cloned function!");

        llvm::ReplaceInstWithInst(
            mappedErrorCall->getParent()->getTerminator(),
            ReturnInst::Create(function->getContext(), mappedErrorCall->getArgOperand(0))
        );

        mappedErrorCall->dropAllReferences();
        mappedErrorCall->eraseFromParent();

        // Map other important stuff: the error PHI node and the may-fail calls.
        auto mappedErrorPhi = dyn_cast_or_null<PHINode>(vmap[info.uniqueErrorPhi]);
        assert(mappedErrorPhi != nullptr && "The error PHI node should be cloned as a PHI node!");

        info.alwaysFailClone = clone;
        info.failCloneErrorPhi = mappedErrorPhi;
        info.mayFailCallsInClone.reserve(info.mayFailCalls.size());

        for (auto call : info.mayFailCalls) {
            info.mayFailCallsInClone.push_back(llvm::cast<CallInst>(vmap[call]));
        }
    }

    // Now that we have the functions, do the call transformations.
    // We want to do this transformation for two kinds of functions:
    // the always failing clones and the main procedure.
    for (auto& entry : mInfos) {
        llvm::Function* function = entry.first;
        auto& info = entry.second;

        if (function == main || !info.canFail()) {
            continue;
        }

        llvm::Function* clone = info.alwaysFailClone;
        splitMayFailCallsInFunction(clone, info.failCloneErrorPhi, info.mayFailCallsInClone);

        // Do a little bit of clean-up: remove the error calls from the original function.
        info.uniqueErrorCall->dropAllReferences();
        info.uniqueErrorCall->eraseFromParent();

        //function->viewCFG();
        //clone->viewCFG();
    }

    // Do the same for main.
    splitMayFailCallsInFunction(main, mInfos[main].uniqueErrorPhi, mInfos[main].mayFailCalls);

    return true;
}

llvm::Pass* gazer::createLiftErrorCallsPass()
{
    return new LiftErrorCallsPass();
}
