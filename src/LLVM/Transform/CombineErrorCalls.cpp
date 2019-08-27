#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Instrumentation/Intrinsics.h"
#include "gazer/LLVM/Instrumentation/Check.h"

#include <llvm/Pass.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Support/CommandLine.h>

using namespace gazer;
using namespace llvm;

cl::opt<bool> NoOverflowChecks("no-overflow-checks", "Do not check for overflow errors.");
cl::opt<bool> NoDivisionByZeroChecks("no-div-zero-checks", "Do not check for divison by zero.");

namespace
{

bool isErrorFunctionName(llvm::StringRef name)
{
    return name == "__VERIFIER_error" || name == "__assert_fail"
        || name == "__gazer_error" ;
}

class CombineErrorCallsPass final : public llvm::ModulePass
{
public:
    static char ID;

    CombineErrorCallsPass()
        : ModulePass(ID)
    {}

    bool runOnModule(llvm::Module& module) override;

    llvm::StringRef getPassName() const override {
        return "Combine Gazer error calls";
    }
};

}

char CombineErrorCallsPass::ID = 0;

bool CombineErrorCallsPass::runOnModule(llvm::Module& module)
{
    auto error = CheckRegistry::GetErrorFunction(module).getCallee();
    llvm::Type* codeTy = llvm::Type::getInt16Ty(module.getContext());

    llvm::IRBuilder<> builder(module.getContext());

    for (llvm::Function& function : module) {
        if (function.isDeclaration()) {
            continue;
        }

        llvm::BasicBlock* errorBlock = llvm::BasicBlock::Create(
            function.getContext(),
            "error",
            &function
        );

        builder.SetInsertPoint(errorBlock);

        auto phi = builder.CreatePHI(codeTy, 0, "error_phi");
        builder.CreateCall(error, { phi });
        builder.CreateUnreachable();

        for (llvm::BasicBlock& bb : function) {
            if (&bb == errorBlock) {
                continue;
            }

            auto it = bb.begin();
            while (it != bb.end()) {
                if (it->getOpcode() == llvm::Instruction::Call) {
                    // Replace error calls to point into a single error block
                    auto call = llvm::dyn_cast<llvm::CallInst>(&*it);
                    llvm::Function* callee = call->getCalledFunction();

                    if (callee != nullptr && callee->getName() == CheckRegistry::ErrorFunctionName) {
                        llvm::Value* code = call->getArgOperand(0);
                        phi->addIncoming(code, &bb);

                        llvm::ReplaceInstWithInst(
                            bb.getTerminator(),
                            llvm::BranchInst::Create(errorBlock)
                        );

                        call->eraseFromParent();
                        break;
                    }
                }

                ++it;
            }
        }
    }

    return true;
} // end anonymous namespace

namespace gazer
{
    llvm::Pass* createCombineErrorCallsPass() {
        return new CombineErrorCallsPass();
    }
} // end namespace gazer