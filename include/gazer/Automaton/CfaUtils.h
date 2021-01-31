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
#ifndef GAZER_AUTOMATON_CFAUTILS_H
#define GAZER_AUTOMATON_CFAUTILS_H

#include "gazer/Automaton/Cfa.h"

#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/Twine.h>

namespace gazer
{

class ExprBuilder;

/// A class which contains the topological sort of a recursive CFA. The mapping is bi-directional:
/// the class knows the index of each location stored in the topological sort.
class CfaTopoSort
{
public:
    CfaTopoSort() = default;
    explicit CfaTopoSort(Cfa& cfa);

    CfaTopoSort(const CfaTopoSort&) = default;
    CfaTopoSort& operator=(const CfaTopoSort&) = default;

    Location* operator[](size_t idx) const;
    size_t indexOf(Location* location) const;
    size_t size() const { return mLocations.size(); }

    using iterator = std::vector<Location*>::const_iterator;
    iterator begin() const { return mLocations.begin(); }
    iterator end() const { return mLocations.end(); }

    template<class LocationIterator>
    iterator insert(size_t idx, LocationIterator first, LocationIterator second)
    {
        auto pos = std::next(mLocations.begin(), idx);
        auto insertPos = mLocations.insert(pos, first, second);

        for (auto it = insertPos, ie = mLocations.end(); it != ie; ++it) {
            size_t currIdx = std::distance(mLocations.begin(), it);
            mLocNumbers[*it] = currIdx;
        }

        return insertPos;
    }

private:
    std::vector<Location*> mLocations;
    llvm::DenseMap<Location*, size_t> mLocNumbers;
};

/// Class for calculating verification path conditions.
class PathConditionCalculator
{
public:
    /// Constructs a new path condition calculator.
    ///
    /// \param topo The topological sort to use.
    /// \param builder An expression builder instance.
    /// \param calls A function which tells the translation process how to represent call
    ///  transitions in the formula.
    /// \param preds If non-null, the formula generation process will insert predecessor information
    ///  into the generated formula. The function passed here will be invoked on each generated
    ///  predecessor expression, so the caller may handle it (e.g. insert it into a map).
    PathConditionCalculator(
        const CfaTopoSort& topo,
        ExprBuilder& builder,
        std::function<ExprPtr(CallTransition*)> calls,
        std::function<void(Location*, ExprPtr)> preds = nullptr
    );

    /// Calculates the reachability condition between \p source and \p target.
    ExprPtr encode(Location* source, Location* target);

private:
    const CfaTopoSort& mTopo;
    ExprBuilder& mExprBuilder;
    std::function<ExprPtr(CallTransition*)> mCalls;
    std::function<void(Location*, ExprPtr)> mPredecessors;
    unsigned mPredIdx = 0;
};

/// Returns the lowest common dominator of each transition in \p targets.
///
/// \param targets A set of target locations.
/// \param topo Topological sort of automaton locations.
/// \param topoIdx A function which returns the index of a location in the topological sort.
/// \param start The start node, which must dominate all target locations. Defaults to the
///     entry location if empty.
Location* findLowestCommonDominator(
    const std::vector<Transition*>& targets,
    const CfaTopoSort& topo,
    Location* start = nullptr
);

/// Returns the highest common post-dominator of each transition in \p targets.
Location* findHighestCommonPostDominator(
    const std::vector<Transition*>& targets,
    const CfaTopoSort& topo,
    Location* start
);

} // namespace gazer

#endif
