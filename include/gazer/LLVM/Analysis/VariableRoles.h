/// \file This file defines the VariableRoles interface.
/// 
/// Variable roles are patterns typical to the uses of a particular value, such
/// as counters, flags, linear arithmetic, comparison roles, etc.
/// Roles are defined through 
/// 
#ifndef GAZER_LLVM_ANALYSIS_VARIABLEROLES_H
#define GAZER_LLVM_ANALYSIS_VARIABLEROLES_H

#include "gazer/Core/Expr.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/CFG.h>
#include <llvm/ADT/DenseMap.h>

#include <vector>

namespace gazer
{

namespace VariableRoles
{

template<class ValueT>
class ValueState
{
public:
    ValueState(ValueT top)
        : mTop(top)
    {}

    ValueT getState(llvm::Value* key)
    {
        auto it = mMap.find(key);
        if (it == mMap.end()) {
            return mTop;
        }

        return it->second;
    }

    void update(llvm::Value* key, ValueT value)
    {
        mMap[key] = std::move(value);
    }

private:
    ValueT mTop;
    llvm::DenseMap<llvm::Value*, ValueT> mMap;
};

template<class ValueT>
class AbstractLattice
{
public:
    AbstractLattice(ValueT top, ValueT bottom)
        : mTop(top), mBottom(bottom)
    {}

    ValueT getTopValue() const { return mTop; }
    ValueT getBottomValue() const { return mBottom; }

    /// Returns the 'meet' result of two values.
    /// Make sure that meet only moves downwards in the lattice.
    virtual ValueT meet(ValueT x, ValueT y) {
        return getBottomValue();
    }

    virtual void transfer(
        llvm::Instruction& inst,
        llvm::DenseMap<llvm::Value*, ValueT>& changed,
        ValueState<ValueT>& state
    ) = 0;

    virtual void printValue(ValueT value, llvm::raw_ostream& os) = 0;

private:

private:
    ValueT mTop;
    ValueT mBottom;
};

template<class ValueT>
class DataflowSolver
{
public:
    explicit DataflowSolver(AbstractLattice<ValueT>* lattice)
        : mLattice(lattice), mState(lattice->getTopValue())
    {}

    DataflowSolver(const DataflowSolver&) = delete;
    DataflowSolver& operator=(const DataflowSolver&) = delete;

    void solve();
    void add(llvm::BasicBlock* bb) {
        mBlockWorkList.push_back(bb);
    }

private:
    void handleInst(llvm::Instruction& inst);
    void handlePHINode(llvm::PHINode& phi, ValueState<ValueT>& state);
    void updateState(llvm::Value* key, ValueT value);

private:
    AbstractLattice<ValueT>* mLattice;

    ValueState<ValueT> mState;
    llvm::SmallVector<llvm::BasicBlock*, 64> mBlockWorkList;
    llvm::SmallVector<llvm::Value*, 64> mValueWorkList;
};

} // end namespace VariableRoles

template<class ValueT>
void VariableRoles::DataflowSolver<ValueT>::handlePHINode(llvm::PHINode& phi, ValueState<ValueT>& state)
{
    ValueT val;
    for (unsigned i = 0, e = phi.getNumIncomingValues(); i != e; ++i) {
        auto incoming = mState.getState(phi.getIncomingValue(i));
        if (incoming != val) {
            val = mLattice->meet(val, incoming);
        }
    }

    updateState(&phi, val);
}

template<class ValueT>
void VariableRoles::DataflowSolver<ValueT>::solve()
{
    while (!mBlockWorkList.empty() || !mValueWorkList.empty()) {
        while (!mValueWorkList.empty()) {
            llvm::Value* value = mValueWorkList.back();
            mValueWorkList.pop_back();

            for (auto user : value->users()) {
                if (auto inst = llvm::dyn_cast<llvm::Instruction>(user)) {
                    handleInst(*inst);
                }
            }
        }

        while (!mBlockWorkList.empty()) {
            llvm::BasicBlock* bb = mBlockWorkList.back();
            mBlockWorkList.pop_back();

            llvm::errs() << bb->getName() << "\n";
            
            for (auto& inst : *bb) {
                handleInst(inst);
            }

            for (auto succ : llvm::successors(bb)) {
                mBlockWorkList.push_back(succ);
            }
        }
    }
}

template<class ValueT>
void VariableRoles::DataflowSolver<ValueT>::handleInst(llvm::Instruction& inst)
{
    // We calculate the PHI node results here, and never pass them
    // to the transfer function.
    if (auto phi = llvm::dyn_cast<llvm::PHINode>(&inst)) {
        return handlePHINode(*phi, mState);
    }

    llvm::DenseMap<llvm::Value*, ValueT> changed;
    mLattice->transfer(inst, changed, mState);
    for (auto& [k, v] : changed) {
        updateState(k, v);
    }
}

template<class ValueT>
void VariableRoles::DataflowSolver<ValueT>::updateState(llvm::Value* key, ValueT value)
{
    ValueT old = mState.getState(key);
    if (value == old) {
        return;
    }

    mState.update(key, std::move(value));
    mValueWorkList.push_back(key);
}

class VariableRoleAnalysis
{
public:
    static std::unique_ptr<VariableRoleAnalysis> Create(llvm::Function* function);
};

}

#endif
