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
#include <llvm/ADT/PostOrderIterator.h>

#include <boost/dynamic_bitset.hpp>

namespace gazer
{

class ExprBuilder;

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
    PathConditionCalculator(
        const CfaTopoSort& topo,
        ExprBuilder& builder,
        std::function<ExprPtr(CallTransition*)> calls,
        std::function<void(Location*, ExprPtr)> preds = nullptr
    );

public:
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

}

#endif
