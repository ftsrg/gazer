#ifndef _GAZER_LLVM_ANALYSIS_VARIABLEROLES_H
#define _GAZER_LLVM_ANALYSIS_VARIABLEROLES_H

#include <llvm/IR/Instruction.h>

#include <bitset>

namespace gazer
{

class VariableRoles
{
public:
    enum UseRole
    {
        Role_UsedInCompare = 0,
        Role_UsedInArithmetic,
        Role_UsedInGetElementPtr
    };

    enum CompareRole
    {
        Role_NeverCompared = 0,
        Role_OnlyComparedToConstant
    };

    static constexpr int LastUseRole = Role_UsedInGetElementPtr;
    static constexpr int LastCompareRole = Role_OnlyComparedToConstant;

    using UseRoleSet = std::bitset<LastUseRole>;
    using CompareRoleSet = std::bitset<LastCompareRole>;

    static VariableRoles calculateRoles(llvm::Instruction& inst);

private:
    llvm::Instruction* mValue;
    UseRoleSet mUseRoles;
    CompareRoleSet mCmpRoles;
};

}

#endif
