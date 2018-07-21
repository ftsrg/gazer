#ifndef _GAZER_LLVM_ANALYSIS_VARIABLEROLES_H
#define _GAZER_LLVM_ANALYSIS_VARIABLEROLES_H

#include <llvm/ADT/APInt.h>
#include <llvm/IR/Function.h>

#include <bitset>
#include <memory>

namespace gazer
{

class VariableRoles
{
public:
    enum Role
    {
        NoRole = 0,
        // This value is only compared to constants
        ComparedOnlyToConstant = 1,
        // This value is never used in a cast operation (SExt, ZExt, ...)
        NeverUsedInCast = 2,
        // This value is only used as a boolean value
        UsedAsBool = 4,
    };

    static constexpr unsigned LastRole = UsedAsBool;

public:
    void add(Role role) {
        mRoleBits |= role;
    }

    void remove(Role role);
    bool has(Role role);

private:
    std::bitset<LastRole> mRoleBits;
};

class VariableRoleAnalysis
{
public:
    static std::unique_ptr<VariableRoleAnalysis> Create(llvm::Function& function);
private:
    llvm::DenseMap<llvm::Value*, VariableRoles> mRoles;
};


}

#endif
