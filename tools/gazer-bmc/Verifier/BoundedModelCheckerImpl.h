//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#ifndef GAZER_SRC_VERIFIER_BOUNDEDMODELCHECKERIMPL_H
#define GAZER_SRC_VERIFIER_BOUNDEDMODELCHECKERIMPL_H

#include "gazer/ADT/ScopedCache.h"
#include "gazer/Automaton/Cfa.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/Expr/ExprEvaluator.h"
#include "gazer/Core/Solver/Model.h"
#include "gazer/Core/Solver/Solver.h"
#include "gazer/Support/Stopwatch.h"
#include "gazer/Trace/Trace.h"
#include "gazer/Verifier/BoundedModelChecker.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/iterator.h>

#include <chrono>
#include <gazer/Automaton/CfaUtils.h>

namespace gazer
{

namespace bmc
{
    using PredecessorMapT = ScopedCache<Location*, ExprPtr>;

    class CexState
    {
    public:
        CexState(Location* location, Transition* incoming)
            : mLocation(location), mOutgoing(incoming)
        {}

        bool operator==(const CexState& rhs) const {
            return mLocation == rhs.mLocation && mOutgoing == rhs.mOutgoing;
        }

        Location* getLocation() const { return mLocation; }
        Transition* getOutgoingTransition() const { return mOutgoing; }

    private:
        Location* mLocation;
        Transition* mOutgoing;
    };

    class BmcCex;

    class cex_iterator :
        public llvm::iterator_facade_base<cex_iterator, std::forward_iterator_tag, CexState>
    {
    public:
        cex_iterator(BmcCex& cex, CexState state)
            : mCex(cex), mState(state)
        {}

        bool operator==(const cex_iterator& rhs) const {
            return mState == rhs.mState;
        }

        const CexState& operator*() const { return mState; }
        CexState& operator*() { return mState; }

        cex_iterator &operator++() {
            this->advance();
            return *this;
        }

    private:
        void advance();

        BmcCex& mCex;
        CexState mState;
    };

    class BmcCex
    {
        friend class cex_iterator;
    public:
        BmcCex(Location* start, Cfa& cfa, ExprEvaluator& eval, PredecessorMapT& preds)
            : mCfa(cfa), mStart(start), mEval(eval), mPredecessors(preds)
        {
            assert(start != nullptr);
        }

        cex_iterator begin() { return cex_iterator(*this, {mStart, nullptr});  }
        cex_iterator end()   { return cex_iterator(*this, {nullptr, nullptr}); }

    private:
        Cfa& mCfa;
        Location* mStart;
        ExprEvaluator& mEval;
        PredecessorMapT& mPredecessors;
    };
} // namespace bmc

class BoundedModelCheckerImpl
{
    struct CallInfo
    {
        ExprPtr overApprox = nullptr;
        std::vector<Cfa*> callChain;

        unsigned getCost() const {
            return std::count(callChain.begin(), callChain.end(), callChain.back());
        }
    };
public:
    struct Stats
    {
        std::chrono::milliseconds SolverTime{0};
        size_t NumInlined = 0;
        size_t NumBeginLocs = 0;
        size_t NumEndLocs = 0;
        size_t NumBeginLocals = 0;
        size_t NumEndLocals = 0;
    };

    BoundedModelCheckerImpl(
        AutomataSystem& system,
        ExprBuilder& builder,
        SolverFactory& solverFactory,
        TraceBuilder<Location*, std::vector<VariableAssignment>>& traceBuilder,
        BmcSettings settings
    );

    std::unique_ptr<VerificationResult> check();

    void printStats(llvm::raw_ostream& os) const;

private:
    void createTopologicalSorts();
    void removeIrrelevantLocations();
    void initializeCallApproximations();
    void performEagerUnrolling();

    bool initializeErrorField();

    void inlineCallIntoRoot(
        CallTransition* call,
        llvm::DenseMap<Variable*, Variable*>& vmap,
        const llvm::Twine& suffix,
        llvm::SmallVectorImpl<CallTransition*>& newCalls
    );

    Solver::SolverStatus underApproximationStep();

    /// Finds the closest common (post-)dominating node for all call transitions.
    /// If no call transitions are present in the CFA, this function returns nullptr.
    std::pair<Location*, Location*> findCommonCallAncestor(Location* fwd, Location* bwd);

    void inlineOpenCalls(Model& model, size_t bound);

    void findOpenCallsInCex(Model& model, llvm::SmallVectorImpl<CallTransition*>& callsInCex);

    std::unique_ptr<VerificationResult> createFailResult();
    std::unique_ptr<Trace> constructTrace(Model& model);

    void push() {
        mSolver->push();
        mPredecessors.push();
    }

    void pop() {
        mPredecessors.pop();
        mSolver->pop();
    }

    Solver::SolverStatus runSolver();

    // Fields
    AutomataSystem& mSystem;
    ExprBuilder& mExprBuilder;
    std::unique_ptr<Solver> mSolver;
    TraceBuilder<Location*, std::vector<VariableAssignment>>& mTraceBuilder;
    BmcSettings mSettings;

    Cfa* mRoot;
    CfaTopoSort mTopo;

    Location* mError = nullptr;
    Location* mTopLoc = nullptr;
    Location* mBottomLoc = nullptr;

    llvm::DenseSet<CallTransition*> mOpenCalls;
    std::unordered_map<CallTransition*, CallInfo> mCalls;
    std::unordered_map<Cfa*, CfaTopoSort> mTopoSortMap;

    std::unique_ptr<PathConditionCalculator> mPathConditions;
    bmc::PredecessorMapT mPredecessors;

    llvm::DenseMap<Location*, Location*> mInlinedLocations;
    llvm::DenseMap<Variable*, Variable*> mInlinedVariables;

    size_t mTmp = 0;
    bool mSkipUnderApprox = false;

    Stats mStats;
    Stopwatch<> mTimer;
    Variable* mErrorFieldVariable = nullptr;
};

} // end namespace gazer

#endif
