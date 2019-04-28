#include "gazer/LLVM/InstrumentationPasses.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;

namespace
{

/**
 * Returns the given instruction casted to CallInst,
 * if it is a malloc call.
 */
llvm::CallInst* getMalloc(Instruction& inst)
{
    if (inst.getOpcode() == Instruction::Call) {
        auto call = llvm::cast<CallInst>(&inst);
        auto callee = call->getCalledFunction();

        if (callee == nullptr) {
            return nullptr;
        }

        if (callee->getName() == "malloc") {
            return call;
        }
    }

    return nullptr;
}

/**
 * This pass inserts the first available address on the heap
 * before every malloc call.
 */
class InsertLastAddressPass final : public ModulePass
{
public:
    static char ID;

    InsertLastAddressPass()
        : ModulePass(ID)
    {}

    virtual void getAnalysisUsage(AnalysisUsage& au) const {
        au.setPreservesCFG();
    }

    virtual bool runOnModule(Module& module)
    {
#if 0
        auto& context = module.getContext();
        IRBuilder<> builder(context);

        // TOOD: This should be based on the target's address widths
        llvm::IntegerType* sizeTy = llvm::Type::getInt64Ty(context);

        // The function 'gazer.malloc' has two arguments:
        // the size to allocate and the size currently allocated on the heap
        llvm::Value* gazerMalloc = module.getOrInsertFunction(
            "gazer.malloc",
            llvm::FunctionType::get(
                llvm::Type::getInt8PtrTy(context, 0),
                { sizeTy, sizeTy },
                false
            )
        );

        for (Function& function : module) {
            if (function.isDeclaration()) {
                continue;
            }

            BasicBlock& entry = function.getEntryBlock();
            builder.SetInsertPoint(&entry, entry.getFirstInsertionPt());

            AllocaInst* alloc = builder.CreateAlloca(sizeTy);
            builder.CreateStore(
                llvm::ConstantInt::get(sizeTy, llvm::APInt(sizeTy->getBitWidth(), 0)),
                alloc
            );

            llvm::DenseSet<CallInst*> mallocCalls;
            for (auto& inst : llvm::instructions(function)) {
                if (auto call = getMalloc(inst)) {
                    mallocCalls.insert(call);
                }
            }

            for (auto call : mallocCalls) {
                // Find the current allocated size
                builder.SetInsertPoint(call);
                LoadInst* load = builder.CreateLoad(alloc);

                // Store the new size
                builder.SetInsertPoint(call->getNextNode());                    
                llvm::Value* addr = builder.CreateAdd(
                    load, call->getArgOperand(0), "addr"
                );
                StoreInst* store = builder.CreateStore(addr, alloc);

                builder.ClearInsertionPoint();
                CallInst* newCall = builder.CreateCall(gazerMalloc, {
                    call->getArgOperand(0),
                    load
                });

                newCall->setDebugLoc(call->getDebugLoc());
                llvm::ReplaceInstWithInst(call, newCall);
            }
        }
#endif
        return true;
    }

    bool runOnFunction(Function& function)
    {
        auto& context = function.getContext();
        IRBuilder<> builder(context);


        

        return true;
    }

};

}

char InsertLastAddressPass::ID = 0;

namespace gazer {
    llvm::Pass* createInsertLastAddressPass() {
        return new InsertLastAddressPass();
    }
}