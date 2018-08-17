#ifndef _GAZER_LLVM_BMC_BMC_H
#define _GAZER_LLVM_BMC_BMC_H

#include "gazer/Core/Solver/Solver.h"
#include "gazer/LLVM/Analysis/TopologicalSort.h"
#include "gazer/LLVM/BMC/BmcTrace.h"

#include <llvm/IR/Function.h>

namespace gazer
{

class BoundedModelCheckerImpl;

class BmcResult final
{
public:
    enum Status
    {
        Safe, Unsafe, Unknown, Timeout, Error
    };

    Status getStatus() const { return mStatus; }
    BmcTrace& getTrace() {
        assert(mStatus == Unsafe && "Can only access trace for unsafe results.");
        return *mTrace;
    }

public:
    static BmcResult CreateSafe() { return BmcResult(Safe); }
    static BmcResult CreateUnsafe() { return BmcResult(Unsafe); }

private:
    BmcResult(Status status, std::unique_ptr<BmcTrace> trace = nullptr)
        : mStatus(status), mTrace(std::move(trace))
    {}
private:
    Status mStatus;
    std::unique_ptr<BmcTrace> mTrace;
};

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

    ~BoundedModelChecker();

public:
    BmcResult run();

private:
    std::unique_ptr<BoundedModelCheckerImpl> pImpl;
};

}

#endif
