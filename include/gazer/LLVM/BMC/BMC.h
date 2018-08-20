#ifndef _GAZER_LLVM_BMC_BMC_H
#define _GAZER_LLVM_BMC_BMC_H

#include "gazer/Core/Solver/Solver.h"
#include "gazer/LLVM/Analysis/TopologicalSort.h"
#include "gazer/LLVM/BMC/BmcTrace.h"

#include <llvm/IR/Function.h>

namespace gazer
{

class BmcResult final
{
public:
    enum Status
    {
        Safe, Unsafe, Unknown, Timeout, Error
    };

    BmcResult(const BmcResult&) = delete;
    BmcResult& operator=(const BmcResult&) = delete;

    Status getStatus() const { return mStatus; }
    BmcTrace& getTrace() {
        assert(mStatus == Unsafe && "Can only access trace for unsafe results.");
        return *mTrace;
    }

public:
    static BmcResult CreateSafe() { return BmcResult(Safe); }
    static BmcResult CreateUnsafe(std::unique_ptr<BmcTrace> trace = nullptr) {
        return BmcResult(Unsafe, std::move(trace));
    }

private:
    BmcResult(Status status, std::unique_ptr<BmcTrace> trace = nullptr)
        : mStatus(status), mTrace(std::move(trace))
    {}
private:
    Status mStatus;
    std::unique_ptr<BmcTrace> mTrace;
};

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
        ExprBuilder* builder,
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
    BmcResult run();

private:
    ExprPtr encodeEdge(llvm::BasicBlock* from, llvm::BasicBlock* to);

private:
    // Basic stuff
    llvm::Function& mFunction;
    TopologicalSort& mTopo;
    SolverFactory& mSolverFactory;
    llvm::raw_ostream& mOS;
    // Symbols and mappings
    SymbolTable mSymbols;
    ExprBuilder* mExprBuilder;

    using VariableToValueMapT = llvm::DenseMap<Variable*, llvm::Value*>;
    VariableToValueMapT mVariableToValueMap;

    InstToExpr::ValueToVariableMapT mVariables;

    using FormulaCacheT = llvm::DenseMap<llvm::BasicBlock*, ExprPtr>;
    FormulaCacheT mFormulaCache;

    // Utils
    InstToExpr mIr2Expr;
};

}

#endif
