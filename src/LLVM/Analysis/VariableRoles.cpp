#include "gazer/LLVM/Analysis/VariableRoles.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>

#include <algorithm>
#include <numeric>

using namespace gazer;
using namespace gazer::VariableRoles;
using namespace llvm;


template<class RoleLattice, class ValueT = decltype(std::declval<RoleLattice>().getTopValue())>
static ValueT MeetOperands(llvm::Instruction& inst, RoleLattice& lattice, ValueState<ValueT>& state)
{
    ValueT result = lattice.getTopValue();
    for (auto& operand : inst.operands()) {
        if (auto i = dyn_cast<Instruction>(&operand)) {
            result = lattice.meet(result, state[i]);
        }
    }

    return result;
}

namespace
{

enum class CompareRoleValue
{
    Never,      ///< Never compared.
    Equals,     ///< Only compared equals or not equals.
    Unsigned,   ///< Only compared equals or unsigned comparison (ult, ...).
    Signed,     ///< Only compared equals or signed comparison (slt, ...).
    Unknown,    ///< No special knowledge about the comparison role.
};

class CompareRoleLattice : public AbstractLattice<CompareRoleValue>
{
    using Role = CompareRoleValue;
public:
    CompareRoleLattice()
        : AbstractLattice(CompareRoleValue::Never, CompareRoleValue::Unknown)
    {}

    Role meet(Role left, Role right) override;

    void transfer(
        Instruction& inst,
        DenseMap<Value*, Role>& changed,
        ValueState<Role>& state
    ) override;

    void printValue(Role value, llvm::raw_ostream& os) override;

    void visitICmpInst(
        ICmpInst& icmp,
        DenseMap<Value*, Role> changed,
        ValueState<Role>& state
    );
};

} // end anonymous namespace

void CompareRoleLattice::printValue(Role value, llvm::raw_ostream& os)
{
    switch (value) {
        case Role::Never: os << "Never"; break;
        case Role::Equals: os << "Equals"; break;
        case Role::Unsigned: os << "Unsigned"; break;
        case Role::Signed: os << "Signed"; break;
        case Role::Unknown: os << "Unknown"; break;
    }
    
    llvm_unreachable("Unknown compare role value!");
}

auto CompareRoleLattice::meet(Role left, Role right) -> Role
{
    if (right == Role::Unknown) {
        return Role::Unknown;
    }

    if (right == Role::Never) {
        return left;
    }

    switch (left) {
        case Role::Unknown:
            return Role::Unknown;
        case Role::Never:
            return right;
        case Role::Equals:
            if (right == Role::Signed)   { return Role::Signed; }
            if (right == Role::Unsigned) { return Role::Unsigned; }

            // 'right' cannot be 'Unknown' or 'Never' at this point
            return Role::Equals;
        case Role::Unsigned:
            if (right == Role::Signed) { return Role::Unknown; }
            return Role::Unsigned;
        case Role::Signed:
            if (left == Role::Unsigned) { return Role::Unknown; }
            return Role::Signed;
    }

    llvm_unreachable("Unknown CompareRole lattice value!");
};

void CompareRoleLattice::transfer(
    Instruction& inst,
    DenseMap<llvm::Value*, Role>& changed,
    ValueState<Role>& state)
{
    if (auto icmp = dyn_cast<ICmpInst>(&inst)) {
        return visitICmpInst(*icmp, changed, state);
    }
}

void CompareRoleLattice::visitICmpInst(
    ICmpInst& icmp,
    DenseMap<Value*, Role> changed,
    ValueState<Role>& state)
{
    for (auto& operand : icmp.operands()) {
        if (auto op = dyn_cast<Instruction>(operand)) {
            if (icmp.isSigned()) {
                changed[op] = Role::Signed;
            } else if (icmp.isUnsigned()) {
                changed[op] = Role::Unsigned;
            } else if (icmp.isEquality()) {
                changed[op] = Role::Equals;
            } else {
                llvm_unreachable("Unknown ICMP predicate!");
            }
        }
    }
}

auto VariableRoleAnalysis::Create(llvm::Function* function) -> std::unique_ptr<VariableRoleAnalysis> 
{
    CompareRoleLattice lattice;
    DataflowSolver<CompareRoleValue> solver(&lattice);

    solver.add(&function->getEntryBlock());
    //solver.solve();

    return nullptr;
}