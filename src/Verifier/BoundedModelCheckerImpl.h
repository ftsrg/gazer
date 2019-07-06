#ifndef GAZER_SRC_VERIFIER_BOUNDEDMODELCHECKERIMPL_H
#define GAZER_SRC_VERIFIER_BOUNDEDMODELCHECKERIMPL_H

#include "gazer/Verifier/BoundedModelChecker.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/Solver/Solver.h"
#include "gazer/Automaton/Cfa.h"

#include "gazer/ADT/ScopedCache.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/DenseSet.h>

#include <chrono>

namespace gazer
{

class BoundedModelCheckerImpl
{
    struct CallInfo
    {
        ExprPtr overApprox = nullptr;
        unsigned cost = 0;
    };

public:
    struct Stats
    {
        std::chrono::milliseconds SolverTime{0};
        int NumInlined = 0;
    };

    BoundedModelCheckerImpl(
        AutomataSystem& system,
        ExprBuilder& builder,
        SolverFactory& solverFactory
    );

    std::unique_ptr<SafetyResult> check();

    void printStats(llvm::raw_ostream& os);

private:
    void inlineCallIntoRoot(
        CallTransition* call,
        llvm::DenseMap<Variable*, Variable*>& vmap,
        llvm::Twine suffix
    );
    
    /// Calculates a the path condition expression between \p source and \p target.
    ExprPtr forwardReachableCondition(Location* source, Location* target);

    Location* findCommonCallAncestor();

    void push() {
        mSolver->push();
        mPredecessors.push();
    }

    void pop() {
        mPredecessors.pop();
        mSolver->pop();
    }

private:
    AutomataSystem& mSystem;
    ExprBuilder& mExprBuilder;
    std::unique_ptr<Solver> mSolver;

    Cfa* mRoot;
    Location* mError;

    std::vector<Location*> mTopo;

    llvm::DenseMap<Location*, size_t> mLocNumbers;
    llvm::DenseMap<CallTransition*, CallInfo> mCalls;
    llvm::DenseSet<CallTransition*> mOpenCalls;
    std::unordered_map<Cfa*, std::vector<Location*>> mTopoSortMap;

    ScopedCache<Location*, std::pair<Variable*, ExprPtr>> mPredecessors;

    llvm::DenseMap<Location*, Location*> mInlinedLocations;
    llvm::DenseMap<Variable*, Variable*> mInlinedVariables;

    size_t mTmp = 0;

    Stats mStats;
};

/// An alternating sequence of {State, Action, ..., State, Action, State }
/// representing a counterexample trace.
class BmcTrace
{
public:
private:
    std::vector<Location*> mStates;
    std::vector<VariableAssignment> mActions;
};

#if 0
class BmcTraceBuilder : public TraceBuilder
{
public:
    BmcTraceBuilder(
        GazerContext& context,
        std::vector<Location*>& topo,
        ScopedCache<Location*, std::pair<Variable*, ExprPtr>>& preds,
        Location* error
    ) :
        mContext(context),
        mTopo(topo), mPreds(preds),
        mError(error)
    {}

protected:
    std::vector<std::unique_ptr<TraceEvent>> buildEvents(Valuation& model) override;

private:

private:
    GazerContext& mContext;
    std::vector<Location*>& mTopo;
    ScopedCache<Location*, std::pair<Variable*, ExprPtr>>&  mPreds;
    Location* mError;
};
#endif

} // end namespace gazer

#endif
