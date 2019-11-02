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

#include "gazer/Verifier/BoundedModelChecker.h"
#include "gazer/Core/Expr/ExprEvaluator.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/Solver/Solver.h"
#include "gazer/Automaton/Cfa.h"
#include "gazer/Trace/Trace.h"

#include "gazer/ADT/ScopedCache.h"

#include <llvm/ADT/iterator.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/DenseSet.h>

#include <chrono>

namespace gazer
{

namespace bmc
{
    using PredecessorMapT = ScopedCache<Location*, std::pair<Variable*, ExprPtr>>;

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

    private:
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
}

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
        unsigned NumInlined = 0;
        unsigned NumBeginLocs = 0;
        unsigned NumEndLocs = 0;
        unsigned NumBeginLocals = 0;
        unsigned NumEndLocals = 0;
    };

    BoundedModelCheckerImpl(
        AutomataSystem& system,
        ExprBuilder& builder,
        SolverFactory& solverFactory,
        TraceBuilder<Location*, std::vector<VariableAssignment>>& traceBuilder,
        BmcSettings settings
    );

    std::unique_ptr<VerificationResult> check();

    void printStats(llvm::raw_ostream& os);

private:
    void createTopologicalSorts();
    bool initializeErrorField();

    void inlineCallIntoRoot(
        CallTransition* call,
        llvm::DenseMap<Variable*, Variable*>& vmap,
        const llvm::Twine& suffix
    );
    
    /// Calculates a the path condition expression between \p source and \p target.
    ExprPtr forwardReachableCondition(Location* source, Location* target);

    /// Finds the closest common ancestor node for all call transitions.
    /// If no call transitions are present in the CFA, this function returns nullptr.
    Location* findCommonCallAncestor();

    void findOpenCallsInCex(Valuation& model, llvm::SmallVectorImpl<CallTransition*>& callsInCex);

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
    TraceBuilder<Location*, std::vector<VariableAssignment>>& mTraceBuilder;
    BmcSettings mSettings;

    Cfa* mRoot;
    std::vector<Location*> mTopo;

    Location* mError = nullptr;

    llvm::DenseMap<Location*, size_t> mLocNumbers;
    llvm::DenseSet<CallTransition*> mOpenCalls;
    std::unordered_map<CallTransition*, CallInfo> mCalls;
    std::unordered_map<Cfa*, std::vector<Location*>> mTopoSortMap;

    bmc::PredecessorMapT mPredecessors;

    llvm::DenseMap<Location*, Location*> mInlinedLocations;
    llvm::DenseMap<Variable*, Variable*> mInlinedVariables;

    size_t mTmp = 0;

    Stats mStats;
    Variable* mErrorFieldVariable = nullptr;
};

std::unique_ptr<Trace> buildBmcTrace(
    const std::vector<Location*>& states,
    const std::vector<std::vector<VariableAssignment>>& actions
);

} // end namespace gazer

#endif
