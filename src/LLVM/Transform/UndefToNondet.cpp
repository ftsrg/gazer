#include "gazer/LLVM/Transform/UndefToNondet.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>

using namespace gazer;

static llvm::FunctionCallee getUndefFunction(llvm::Type* type, llvm::Module* module)
{
    std::string nameBuffer;
    llvm::raw_string_ostream rso(nameBuffer);
    type->print(rso, false, true);
    rso.flush();

    auto name = UndefToNondetCallPass::UndefValueFunctionPrefix + rso.str();

    return module->getOrInsertFunction(name, llvm::FunctionType::get(type, false));
}

static bool replaceUndefsWithCalls(llvm::Function& function)
{
    bool changed = false;
    auto module = function.getParent();

    for (llvm::BasicBlock& bb : function) {
        for (llvm::Instruction& inst : bb) {
            for (size_t i = 0 ; i < inst.getNumOperands(); ++i) {
                auto operand = inst.getOperand(i);
                if (auto undef = llvm::dyn_cast<llvm::UndefValue>(operand)) {
                    llvm::FunctionCallee func = getUndefFunction(undef->getType(), module);

                    // Found an undef, insert an instruction.
                    llvm::CallInst* call = llvm::CallInst::Create(func.getFunctionType(), func.getCallee(), "undefv");
                    call->copyMetadata(inst);

                    if (auto phi = llvm::dyn_cast<llvm::PHINode>(&inst)) {
                        // If the instruction is a PHI node, insert the call before the terminator of the
                        // corresponding predecessor block.
                        auto pred = phi->getIncomingBlock(i);
                        pred->getInstList().insert(pred->getTerminator()->getIterator(), call);
                    } else {
                        // Otherwise we can just insert it before the current instruction.
                        bb.getInstList().insert(inst.getIterator(), call);
                    }

                    inst.setOperand(i, call);

                    changed |= true;
                }
            }
        }
    }

    return changed;
}

char UndefToNondetCallPass::ID;

bool UndefToNondetCallPass::runOnModule(llvm::Module& module)
{
    bool changed = false;

    for (llvm::Function& function : module) {
        changed |= replaceUndefsWithCalls(function);
    }

    return changed;
}
