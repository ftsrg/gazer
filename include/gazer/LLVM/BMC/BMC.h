#ifndef GAZER_LLVM_BMC_BMC_H
#define GAZER_LLVM_BMC_BMC_H

#include "gazer/Core/Solver/Solver.h"
#include "gazer/LLVM/Analysis/TopologicalSort.h"
#include "gazer/LLVM/BMC/BmcTrace.h"
#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include "gazer/Trace/SafetyResult.h"

#include <llvm/IR/Function.h>

namespace gazer
{

class MemoryModel;

/**
 * Implements a simple bounded model checker algorithm.
 */
class BoundedModelChecker final
{
public:
    /**
     * Initializes a bounded model checker for a given function.
     */
    BoundedModelChecker(
        llvm::Function& function,
        TopologicalSort& topo,
        GazerContext& context,
        ExprBuilder* builder,
        legacy::MemoryModel& memoryModel,
        SolverFactory& solverFactory,
        llvm::raw_ostream& os = llvm::nulls()
    );

    BoundedModelChecker(const BoundedModelChecker&) = delete;
    BoundedModelChecker& operator=(const BoundedModelChecker&) = delete;
    
    using ProgramEncodeMapT = llvm::SmallDenseMap<llvm::BasicBlock*, ExprPtr, 1>;
    
    /**
     * Encodes the program into an SMT formula.
     */
    ProgramEncodeMapT encode();

    /**
     * Solves an already encoded program formula.
     */
    std::unique_ptr<SafetyResult> run();

private:
    ExprPtr encodeEdge(llvm::BasicBlock* from, llvm::BasicBlock* to);

private:
    // Basic stuff
    llvm::Function& mFunction;
    TopologicalSort& mTopo;
    SolverFactory& mSolverFactory;
    llvm::raw_ostream& mOS;
    // Symbols and mappings
    GazerContext& mContext;
    ExprBuilder* mExprBuilder;

    using VariableToValueMapT = llvm::DenseMap<Variable*, llvm::Value*>;
    VariableToValueMapT mVariableToValueMap;

    ValueToVariableMap mVariables;

    using FormulaCacheT = llvm::DenseMap<llvm::BasicBlock*, ExprPtr>;
    FormulaCacheT mFormulaCache;

    // Utils
    llvm::DenseMap<llvm::Value*, ExprPtr> mEliminatedVars;
    llvm::DenseMap<llvm::BasicBlock*, ExprPtr> mPredecessors;

    // This should be the last one to be initialized.
    InstToExpr mIr2Expr;
};

}

#endif
