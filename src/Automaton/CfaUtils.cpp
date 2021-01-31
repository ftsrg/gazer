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
#include "gazer/Automaton/CfaUtils.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/Expr/ExprRewrite.h"

#include <llvm/ADT/PostOrderIterator.h>

#include <boost/dynamic_bitset.hpp>

using namespace gazer;

// Topological sorts
//===----------------------------------------------------------------------===//
CfaTopoSort::CfaTopoSort(Cfa &cfa)
{
    auto poBegin = llvm::po_begin(cfa.getEntry());
    auto poEnd = llvm::po_end(cfa.getEntry());

    mLocations.reserve(cfa.getNumLocations());
    mLocations.insert(mLocations.end(), poBegin, poEnd);
    std::reverse(mLocations.begin(), mLocations.end());

    mLocNumbers.reserve(mLocations.size());
    for (size_t i = 0; i < mLocations.size(); ++i) {
        mLocNumbers[mLocations[i]] = i;
    }
}

Location* CfaTopoSort::operator[](size_t idx) const
{
    assert(idx >= 0 && idx < mLocations.size() && "Out of bounds access on a topological sort!");
    return mLocations[idx];
}

size_t CfaTopoSort::indexOf(Location *location) const
{
    auto it = mLocNumbers.find(location);
    assert(it != mLocNumbers.end() && "Attempting to fetch a non-existing index from a topological sort!");

    return it->second;
}


// Calculating path conditions
//===----------------------------------------------------------------------===//

PathConditionCalculator::PathConditionCalculator(
    const CfaTopoSort& topo,
    ExprBuilder& builder,
    std::function<ExprPtr(CallTransition*)> calls,
    std::function<void(Location*, ExprPtr)> preds
) : mTopo(topo), mExprBuilder(builder), mCalls(calls),
    mPredecessors(preds)
{}

namespace
{

struct PathPredecessor
{
    Transition* edge;
    size_t idx;
    ExprPtr expr;

    PathPredecessor() = default;

    PathPredecessor(Transition* edge, size_t idx, ExprPtr expr)
        : edge(edge), idx(idx), expr(expr)
    {}
};

} // end anonymous namespace

ExprPtr PathConditionCalculator::encode(Location* source, Location* target)
{
    if (source == target) {
        return mExprBuilder.True();
    }

    size_t startIdx = mTopo.indexOf(source);
    size_t targetIdx = mTopo.indexOf(target);

    auto& ctx = mExprBuilder.getContext();
    assert(startIdx < targetIdx && "The source location must be before the target in a topological sort!");
    assert(targetIdx < mTopo.size() && "The target index is out of range in the VC array!");

    std::vector<ExprPtr> dp(targetIdx - startIdx + 1);

    std::fill(dp.begin(), dp.end(), mExprBuilder.False());

    // The first location is always reachable from itself.
    dp[0] = mExprBuilder.True();

    for (size_t i = 1; i < dp.size(); ++i) {
        Location* loc = mTopo[i + startIdx];
        ExprVector exprs;

        llvm::SmallVector<PathPredecessor, 16> preds;
        for (Transition* edge : loc->incoming()) {
            size_t predIdx = mTopo.indexOf(edge->getSource());
            assert(predIdx < i + startIdx
                && "Predecessors must be before block in a topological sort. "
                "Maybe there is a loop in the automaton?");

            if (predIdx >= startIdx) {
                // We are skipping the predecessors which are outside the region we are interested in.
                ExprPtr formula = mExprBuilder.And({
                    dp[predIdx - startIdx],
                    edge->getGuard()
                });

                if (auto assignEdge = llvm::dyn_cast<AssignTransition>(edge)) {
                    ExprVector assigns;

                    for (auto& assignment : *assignEdge) {
                        // As we are dealing with an SSA-formed CFA, we can just omit undef assignments.
                        if (assignment.getValue()->getKind() != Expr::Undef) {
                            auto eqExpr = mExprBuilder.Eq(assignment.getVariable()->getRefExpr(), assignment.getValue());
                            assigns.push_back(eqExpr);
                        }
                    }

                    if (!assigns.empty()) {
                        formula = mExprBuilder.And(formula, mExprBuilder.And(assigns));
                    }
                } else if (auto callEdge = llvm::dyn_cast<CallTransition>(edge)) {
                    formula = mExprBuilder.And(formula, mCalls(callEdge));
                }
                
                preds.emplace_back(edge, predIdx, formula);
            }
        }

        if (LLVM_UNLIKELY(preds.empty())) {
            dp[i] = mExprBuilder.False();
        } else if (preds.size() == 1) {
            if (mPredecessors != nullptr) {
                mPredecessors(loc, mExprBuilder.IntLit(preds[0].edge->getSource()->getId()));
            }
            dp[i] = preds[0].expr;
        } else if (preds.size() == 2) {
            ExprPtr p1 = mExprBuilder.True();
            ExprPtr p2 = mExprBuilder.True();

            if (mPredecessors != nullptr) {
                Variable* predDisc = ctx.createVariable(
                    "__gazer_pred_" + std::to_string(mPredIdx++), BoolType::Get(ctx)
                );

                unsigned first  = preds[0].edge->getSource()->getId();
                unsigned second = preds[1].edge->getSource()->getId();

                mPredecessors(loc, mExprBuilder.Select(
                    predDisc->getRefExpr(), mExprBuilder.IntLit(first), mExprBuilder.IntLit(second)
                ));

                p1 = predDisc->getRefExpr();
                p2 = mExprBuilder.Not(predDisc->getRefExpr());
            }

            dp[i] = mExprBuilder.Or(
                mExprBuilder.And(preds[0].expr, p1),
                mExprBuilder.And(preds[1].expr, p2)
            );
        } else {
            Variable* predDisc = nullptr;
            if (mPredecessors != nullptr) {
                predDisc = ctx.createVariable(
                    "__gazer_pred_" + std::to_string(mPredIdx++), IntType::Get(ctx)
                );
                mPredecessors(loc, predDisc->getRefExpr());
            }

            for (size_t j = 0; j < preds.size(); ++j) {
                Transition* edge = preds[j].edge;
                size_t predIdx = preds[j].idx;

                ExprPtr predIdentification = mExprBuilder.True();

                if (predDisc != nullptr) {
                    predIdentification = mExprBuilder.Eq(
                        predDisc->getRefExpr(),
                        mExprBuilder.IntLit(preds[j].edge->getSource()->getId())
                    );
                }

                ExprPtr formula = mExprBuilder.And({
                    preds[j].expr,
                    predIdentification
                });

                exprs.push_back(formula);
            }

            dp[i] = mExprBuilder.Or(exprs);
        }
    }

    return dp.back();
}

// Lowest common dominators
//===----------------------------------------------------------------------===//

Location* gazer::findLowestCommonDominator(
    const std::vector<Transition*>& targets,
    const CfaTopoSort& topo,
    Location* start)
{
    if (targets.empty()) {
        // There cannot be a suitable ancestor, just return the start node.
        return nullptr;
    }

    if (start == nullptr) {
        start = topo[0];
    }

    // Find the last interesting index in the topological sort.
    auto end = std::max_element(targets.begin(), targets.end(), [&topo](auto& a, auto& b) {
      return topo.indexOf(a->getSource()) < topo.indexOf(b->getSource());
    });

    size_t startIdx = topo.indexOf(start);
    size_t lastIdx  = topo.indexOf((*end)->getTarget());

    assert(lastIdx > startIdx && "The last interesting index must be larger than the start index!");

    // Count the number of locations between start and last in the topological sort.
    size_t numLocs = lastIdx - startIdx;

    // We will calculate dominators in one go, exploiting that the graph is guaranteed to be
    // a DAG and that we already have the topological sort. We will use the standard definition:
    //      Dom(n_0) = { n_0 }
    //      Dom(n) = Union({ n }, Intersect({ p in pred(n): Dom(p) }))
    // To represent the Dom sets for each node n, we will use a bitset, where a bit i is set if
    // topo[i] dominates n.
    std::vector<boost::dynamic_bitset<>> dominators(numLocs, boost::dynamic_bitset(numLocs));
    dominators[0][0] = true;

    for (size_t i = 1; i < numLocs; ++i) {
        Location* loc = topo[startIdx + i];

        boost::dynamic_bitset<> bs(numLocs);
        bs.set();
        for (Transition* edge : loc->incoming()) {
            size_t predIdx = topo.indexOf(edge->getSource());
            assert(predIdx < i + startIdx
                && "Predecessors must be before node in a topological sort. "
                "Maybe there is a loop in the automaton?");

            if (predIdx < startIdx) {
                // We are skipping the predecessors we are not interested in.
                // Note that this is only safe because we *know* that `start`
                // dominates each target, therefore all initial paths to the
                // targets must already go through `start`.
                continue;
            }

            bs = bs & dominators[predIdx - startIdx];
        }
        bs[i] = true;
        dominators[i] = bs;
    }

    // Now that we have the dominators, find the common dominators for the target edges.
    boost::dynamic_bitset<> commonDominators(numLocs);
    commonDominators.set();
    for (Transition* edge : targets) {
        size_t idx = topo.indexOf(edge->getSource());
        commonDominators = commonDominators & dominators[idx - startIdx];
    }

    assert(commonDominators.test(0)
        && "There must be at least one possible common dominator (the start location)!");

    // Find the highest set bit
    size_t commonDominatorIndex = 0;
    for (size_t i = commonDominators.size() - 1; i > 0; --i) {
        if (commonDominators[i]) {
            commonDominatorIndex = i;
            break;
        }
    }

    return topo[commonDominatorIndex + startIdx];
}

Location* gazer::findHighestCommonPostDominator(
    const std::vector<Transition*>& targets,
    const CfaTopoSort& topo,
    Location* start
) {

    if (targets.empty()) {
        // There cannot be a suitable ancestor, just return the start node.
        return nullptr;
    }

    if (start == nullptr) {
        start = targets[0]->getSource()->getAutomaton()->getExit();
    }

    // Find the last interesting index in the topological sort.
    auto end = std::min_element(targets.begin(), targets.end(), [&topo](auto& a, auto& b) {
      return topo.indexOf(a->getSource()) < topo.indexOf(b->getSource());
    });

    size_t startIdx = topo.indexOf(start);
    size_t lastIdx  = topo.indexOf((*end)->getSource());

    assert(lastIdx < startIdx && "The last interesting index must be larger than the start index!");

    // Count the number of locations between start and last in the topological sort.
    size_t numLocs = startIdx - lastIdx;

    // We will calculate dominators in one go, exploiting that the graph is guaranteed to be
    // a DAG and that we already have the topological sort. We will use the standard definition:
    //      Dom(n_0) = { n_0 }
    //      Dom(n) = Union({ n }, Intersect({ p in pred(n): Dom(p) }))
    // To represent the Dom sets for each node n, we will use a bitset, where a bit i is set if
    // topo[i] dominates n.
    std::vector<boost::dynamic_bitset<>> dominators(numLocs, boost::dynamic_bitset(numLocs));
    dominators[0][0] = true;

    for (size_t i = 1; i < numLocs; ++i) {
        Location* loc = topo[startIdx - i];

        boost::dynamic_bitset<> bs(numLocs);
        bs.set();
        for (Transition* edge : loc->outgoing()) {
            size_t succIdx = topo.indexOf(edge->getTarget());

            if (succIdx > startIdx) {
                // We are skipping the predecessors we are not interested in.
                // Note that this is only safe because we *know* that `start`
                // dominates each target, therefore all initial paths to the
                // targets must already go through `start`.
                continue;
            }

            bs = bs & dominators[startIdx - succIdx];
        }
        bs[i] = true;
        dominators[i] = bs;
    }

    // Now that we have the dominators, find the common dominators for the target edges.
    boost::dynamic_bitset<> commonDominators(numLocs);
    commonDominators.set();
    for (Transition* edge : targets) {
        size_t idx = topo.indexOf(edge->getTarget());
        commonDominators = commonDominators & dominators[startIdx - idx];
    }

    assert(commonDominators.test(0)
        && "There must be at least one possible common dominator (the start location)!");

    // Find the highest set bit
    size_t commonDominatorIndex = 0;
    for (size_t i = commonDominators.size() - 1; i > 0; --i) {
        if (commonDominators[i]) {
            commonDominatorIndex = i;
            break;
        }
    }

    return topo[startIdx - commonDominatorIndex];
}
