#ifndef _GAZER_LLVM_BMC_BMC_H
#define _GAZER_LLVM_BMC_BMC_H

#include "gazer/Core/Solver/Solver.h"
#include "gazer/LLVM/Analysis/TopologicalSort.h"
#include "gazer/LLVM/BMC/BmcTrace.h"
#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include "gazer/Trace/SafetyResult.h"

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

    std::optional<unsigned> getErrorType() const {
        return mErrorCode;
    }

    bool isSafe() const { return mStatus == Safe; }
    bool isUnsafe() const { return mStatus == Unsafe; }

public:
    static BmcResult CreateSafe() { return BmcResult(Safe); }
    static BmcResult CreateUnsafe(unsigned ec, std::unique_ptr<BmcTrace> trace = nullptr) {
        return BmcResult(Unsafe, ec, std::move(trace));
    }

private:
    BmcResult(Status status, std::unique_ptr<BmcTrace> trace = nullptr)
        : mStatus(status), mTrace(std::move(trace))
    {}

    BmcResult(
        Status status,
        unsigned errorCode,
        std::unique_ptr<BmcTrace> trace = nullptr
    )
        : mStatus(status), mTrace(std::move(trace)), mErrorCode(errorCode)
    {}
private:
    Status mStatus;
    std::unique_ptr<BmcTrace> mTrace;
    std::optional<unsigned> mErrorCode;
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
    SymbolTable mSymbols;
    ExprBuilder* mExprBuilder;

    using VariableToValueMapT = llvm::DenseMap<Variable*, llvm::Value*>;
    VariableToValueMapT mVariableToValueMap;

    InstToExpr::ValueToVariableMapT mVariables;

    using FormulaCacheT = llvm::DenseMap<llvm::BasicBlock*, ExprPtr>;
    FormulaCacheT mFormulaCache;

    // Utils
    llvm::DenseMap<llvm::Value*, ExprPtr> mEliminatedVars;

    // This should be the last one to be initialized.
    InstToExpr mIr2Expr;
};

}

#endif
