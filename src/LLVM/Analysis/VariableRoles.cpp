#include "gazer/LLVM/Analysis/VariableRoles.h"

using namespace gazer;
using namespace llvm;

VariableRoles VariableRoles::calculateRoles(llvm::Instruction& inst)
{
    UseRoleSet useRS;
    CompareRoleSet cmpRS;

    for (llvm::Use& use : inst.uses()) {
        if (auto user = dyn_cast<Instruction>(use.getUser())) {
            unsigned opcode = user->getOpcode();
            if (opcode == Instruction::ICmp || opcode == Instruction::FCmp) {
                useRS[Role_UsedInCompare] = 1;
                cmpRS[Role_NeverCompared] = 0;

                unsigned otherOp = use.getOperandNo() == 1 ? 0 : 1;
                if (!isa<ConstantData>(user->getOperand(otherOp))) {
                    cmpRS[Role_OnlyComparedToConstant] = 0;
                }
            }
        }
    }

};
