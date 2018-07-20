#include "gazer/LLVM/Transform/Passes.h"

#include <llvm/Pass.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

namespace
{

bool isErrorFunctionName(llvm::StringRef name)
{
    return name == "__VERIFIER_error" || name == "__assert_fail"
        || name == "__gazer_error";
}

bool isErrorBlock(const llvm::BasicBlock& bb)
{
    for (auto& instr : bb) {
        if (instr.getOpcode() == llvm::Instruction::Call) {
            auto call = llvm::dyn_cast<llvm::CallInst>(&instr);
            llvm::Function* callee = call->getCalledFunction();

            if (isErrorFunctionName(callee->getName())) {
                return true;
            }
        }
    }

    return false;
}

class CombineErrorCallsPass final : public llvm::ModulePass
{
public:
    static char ID;

    CombineErrorCallsPass()
        : ModulePass(ID)
    {}

    bool runOnModule(llvm::Module& module) override
    {
        llvm::Constant* error = module.getOrInsertFunction(
            "__gazer_error",
            llvm::Type::getVoidTy(module.getContext())
        );

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
            builder.CreateCall(error);
            builder.CreateUnreachable();

            for (llvm::BasicBlock& bb : function) {
                if (&bb == errorBlock) {
                    continue;
                }

                auto it = bb.begin();
                while (it != bb.end()) {
                    if (it->getOpcode() == llvm::Instruction::Call) {
                        auto call = llvm::dyn_cast<llvm::CallInst>(&*it);
                        llvm::Function* callee = call->getCalledFunction();

                        if (isErrorFunctionName(callee->getName())) {
                            llvm::ReplaceInstWithInst(
                                bb.getTerminator(),
                                llvm::BranchInst::Create(
                                    errorBlock
                                )
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
    }
};

}

char CombineErrorCallsPass::ID = 0;

namespace gazer {
    llvm::Pass* createCombineErrorCallsPass() {
        return new CombineErrorCallsPass();
    }
}