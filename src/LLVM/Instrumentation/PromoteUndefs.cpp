#include <llvm/Pass.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>

#include <llvm/ADT/DenseMap.h>

namespace
{

bool TransformBlock(llvm::Module& module, llvm::BasicBlock& bb)
{
    bool changed = false;
    llvm::IRBuilder<> builder(module.getContext());

    unsigned funCnt = 0;
    llvm::DenseMap<llvm::Type*, llvm::Function*> nondetFunctions;

    for (llvm::Instruction& inst : bb) {
        for (size_t i = 0; i < inst.getNumOperands(); ++i) {
            llvm::Value* value = inst.getOperand(i);

            //if (auto md = llvm::dyn_cast<llvm::MetadataAsValue>(value)) {
            //    if (auto mdVal = llvm::dyn_cast<llvm::ValueAsMetadata>(md->getMetadata())) {
            //        value = mdVal->getValue();
            //    }
            //}

            if (auto undef = llvm::dyn_cast<llvm::UndefValue>(value)) {
                llvm::Function* function = nullptr;
                llvm::Type* type = undef->getType();

                auto result = nondetFunctions.find(type);
                if (result != nondetFunctions.end()) {
                    function = result->second;
                } else {
                    std::string name = "__gazer_nondet_" + std::to_string(funCnt++);
                    auto newFun = module.getOrInsertFunction(name, type);

                    function = llvm::dyn_cast<llvm::Function>(newFun);
                    nondetFunctions[type] = function;
                }

                // Insert the call instruction before the current one
                auto call = builder.CreateCall(function, {}, "undef");
                call->insertBefore(&inst);
                inst.setOperand(i, call);

                changed = true;
            }
        }
    }

    return changed;
}

/**
 * This pass promotes 'undef' operands into non-determistic function calls.
 */
struct PromoteUndefsPass : public llvm::ModulePass
{
    static char ID;

    PromoteUndefsPass()
        : ModulePass(ID)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override
    {
        au.setPreservesCFG();
    }

    bool runOnModule(llvm::Module &module) override
    {
        bool changed = false;

        for (llvm::Function& function : module) {
            for (llvm::BasicBlock& bb : function) {
                changed |= TransformBlock(module, bb);
            }
        }

        return changed;
    };
};

}

char PromoteUndefsPass::ID = 0;

namespace gazer {
    llvm::Pass* createPromoteUndefsPass() { return new PromoteUndefsPass(); }
}
