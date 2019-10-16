/// \file This file defines the LiftErrorsPass, which lifts error calls
/// from loops and subroutines into the main module.
#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Instrumentation/Check.h"

#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#include <unordered_map>

using namespace gazer;
using namespace llvm;

namespace
{

class LiftErrorCalls
{
    struct FunctionInfo
    {
        llvm::DenseSet<llvm::CallInst*> selfFails;
        llvm::PHINode* uniqueErrorPhi;
        llvm::CallInst* uniqueErrorCall;
    };
public:
    LiftErrorCalls(llvm::Module& module, llvm::CallGraph& cg)
        : mModule(module),
        mCallGraph(cg), mBuilder(module.getContext())
    {}

    bool run();

private:
    void combineErrorsInFunction(llvm::Function* function, FunctionInfo& info);

private:
    llvm::Module& mModule;
    llvm::CallGraph& mCallGraph;
   // llvm::LoopInfo& mLoopInfo;
    llvm::IRBuilder<> mBuilder;
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

    llvm::StringRef getPassName() const override { return "Lift error calls into main."; }
};

} // end anonymous namespace

char LiftErrorCallsPass::ID;

void LiftErrorCalls::combineErrorsInFunction(llvm::Function* function, FunctionInfo& info)
{
    auto error = CheckRegistry::GetErrorFunction(mModule).getCallee();
    llvm::Type* errorCodeTy = CheckRegistry::GetErrorCodeType(mModule.getContext());

    auto errorBlock = llvm::BasicBlock::Create(function->getContext(), "error", function);
    mBuilder.SetInsertPoint(errorBlock);

    auto phi = mBuilder.CreatePHI(errorCodeTy, info.selfFails.size(), "error_phi");
    auto err = mBuilder.CreateCall(error, { phi });
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
    bool changed = false;
    llvm::DenseMap<llvm::Function*, FunctionInfo> functions;

    // First, collect all intraprocedural information.
    for (llvm::Function& function : mModule) {
        if (function.isDeclaration()) {
            continue;
        }

        for (auto& inst : llvm::instructions(function)) {
            if (auto call = llvm::dyn_cast<CallInst>(&inst)) {
                llvm::Function* callee = call->getCalledFunction();
                if (callee != nullptr && callee->getName() == CheckRegistry::ErrorFunctionName) {
                    functions[&function].selfFails.insert(call);
                }
            }
        }
    }

    // Transform interprocedurally: create a single, unique error call for
    // each function that may fail. Previous error calls will point to
    // this single error location afterwards.
    for (auto& entry : functions) {
        combineErrorsInFunction(entry.first, entry.second);
    }

    return true;
#if 0
    auto errorFunc = CheckRegistry::GetErrorFunction(mModule).getCallee();

    llvm::IRBuilder<> builder(mModule.getContext());
    llvm::DenseMap<const llvm::Function*, FunctionInfo> errorFunctions;

    // Create a unique error block for each function.
    auto cgBegin = llvm::po_begin(&mCallGraph);
    auto cgEnd = llvm::po_end(&mCallGraph);

    // TODO: Will this guarantee that we discover everything?
    for (auto it = cgBegin; it != cgEnd; ++it) {
        auto cgNode = *it;
        llvm::Function* function = cgNode->getFunction();
        bool hasErrors = false;

        // Check if we have any calls to a function which may produce errors.
        for (auto& callRecord : *cgNode) {
            llvm::Function* calledFunction = callRecord.second->getFunction();
            if (errorFunctions.count(calledFunction) != 0) {
                hasErrors = true;
                errorFunctions[function].failingCalls.insert(callRecord);
            }
        }

        // Find all error blocks.
        for (llvm::Instruction& inst : llvm::instructions(function)) {
            if (auto call = llvm::dyn_cast<CallInst>(&inst)) {
                const llvm::Function* callee = call->getCalledFunction();
                if (callee != nullptr && callee->getName() == CheckRegistry::ErrorFunctionName) {
                    errorFunctions[function].selfFails.insert(call);
                    hasErrors = true;
                }
            }
        }
    }

    auto errorCodeTy = llvm::Type::getInt16Ty(mModule.getContext());

    for (auto& entry : errorFunctions) {
        const llvm::Function* origFunction = entry.first;

        // We will have two versions of each function: one that always fails, and one that cannot fail.
        // The failing clone will have the same operand list as the original, but the return type shall
        // be the error code.
        auto failFuncTy = llvm::FunctionType::get(
            errorCodeTy, origFunction->getFunctionType()->params(), origFunction->getFunctionType()->isVarArg()
        );

        auto alwaysFailClone = llvm::Function::Create(
            failFuncTy, origFunction->getLinkage(), origFunction->getName() + "_fail", &mModule
        );

        // Perform the cloning.
        llvm::ValueToValueMapTy vmap;
        llvm::SmallVector<ReturnInst*, 4> returns;
        llvm::CloneFunctionInto(alwaysFailClone, origFunction, vmap, false, returns);

        // The cloned function is in a invalid state, as the return type will probably differ from the original.
        // Replace all returns with unreachable's.
        for (auto ret : returns) {
            llvm::ReplaceInstWithInst(ret, new UnreachableInst(mModule.getContext()));
        }

        // Add a new return block
        auto errorBlock = llvm::BasicBlock::Create(
            origFunction->getContext(), "error", alwaysFailClone
        );

        builder.SetInsertPoint(errorBlock);
        auto phi = builder.CreatePHI(errorCodeTy, entry., "error_phi");
    }
#endif
}

llvm::Pass* gazer::createLiftErrorCallsPass()
{
    return new LiftErrorCallsPass();
}
