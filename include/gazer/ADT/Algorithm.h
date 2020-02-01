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
#ifndef GAZER_ADT_ALGORITHM_H
#define GAZER_ADT_ALGORITHM_H

#include <boost/dynamic_bitset.hpp>

namespace gazer
{

/// A quadratic-time algorithm to find the intersection and difference of two ranges.
/// Given two containers A and B, this function finds the sets A * B, A / B, B / A.
/// If duplicates are present, they will not be inserted into either output set.
template<class Range1, class Range2, class OutputIt1, class OutputIt2, class OutputIt3>
void intersection_difference(
    Range1 left, Range2 right, OutputIt1 commonOut, OutputIt2 lMinusROut, OutputIt3 rMinusLOut)
{
    size_t leftSiz  = std::distance(left.begin(), left.end());
    size_t rightSiz = std::distance(right.begin(), right.end());

    boost::dynamic_bitset<> rightElems(rightSiz);
    
    for (size_t i = 0; i < leftSiz; ++i) {
        auto li = std::next(left.begin(), i);
        bool isPresentInRight = false;

        for (size_t j = 0; j < rightSiz; ++j) {
            auto rj = std::next(right.begin(), j);
            if (*li == *rj) {
                if (!rightElems[j] && !isPresentInRight) {
                    *(commonOut++) = *li;
                }
                isPresentInRight = true;
                rightElems[j] = true;
            }
        }

        if (!isPresentInRight) {
            *(lMinusROut++) = *li;
        }
    }
    
    // Do another pass to find unpaired elements in right
    for (size_t j = 0; j < rightSiz; ++j) {
        auto rj = std::next(right.begin(), j);
        if (!rightElems[j]) {
            *(rMinusLOut++) = *rj;
        }
    }
}

} // end namespace gazer

#endif
