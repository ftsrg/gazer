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
#include "gazer/ADT/Algorithm.h"

#include <llvm/ADT/iterator_range.h>

#include <gtest/gtest.h>

namespace
{

TEST(IntersectionDifferenceTest, Simple)
{
    std::vector<int> v1 = {1,2,3,4,5,6,7,8};
    std::vector<int> v2 = {        5,  7,  9,10};

    auto range1 = llvm::make_range(v1.begin(), v1.end());
    auto range2 = llvm::make_range(v2.begin(), v2.end());

    std::vector<int> intersection;
    std::vector<int> v1_minus_v2;
    std::vector<int> v2_minus_v1;

    gazer::intersection_difference(
        range1, range2, std::back_inserter(intersection),
        std::back_inserter(v1_minus_v2), std::back_inserter(v2_minus_v1)
    );

    ASSERT_EQ(intersection.size(), 2);
    ASSERT_EQ(v1_minus_v2.size(), 6);
    ASSERT_EQ(v2_minus_v1.size(), 2);

    std::vector<int> expected_intersection = {5, 7};
    std::vector<int> expected_v1_minus_v2 = {1, 2, 3, 4, 6, 8};
    std::vector<int> expected_v2_minus_v1 = {9, 10};

    ASSERT_EQ(intersection, expected_intersection);
    ASSERT_EQ(v1_minus_v2,  expected_v1_minus_v2);
    ASSERT_EQ(v2_minus_v1,  expected_v2_minus_v1);
}

TEST(IntersectionDifferenceTest, RightEmpty)
{
    std::vector<int> v1 = {1,2,3,4,5,6,7,8};
    std::vector<int> v2 = {};

    auto range1 = llvm::make_range(v1.begin(), v1.end());
    auto range2 = llvm::make_range(v2.begin(), v2.end());

    std::vector<int> intersection;
    std::vector<int> v1_minus_v2;
    std::vector<int> v2_minus_v1;

    gazer::intersection_difference(
        range1, range2, std::back_inserter(intersection),
        std::back_inserter(v1_minus_v2), std::back_inserter(v2_minus_v1)
    );

    ASSERT_EQ(intersection.size(), 0);
    ASSERT_EQ(v1_minus_v2.size(), 8);
    ASSERT_EQ(v2_minus_v1.size(), 0);

    std::vector<int> expected_v1_minus_v2 = {1,2,3,4,5,6,7,8};

    ASSERT_EQ(v1_minus_v2,  expected_v1_minus_v2);
}

TEST(IntersectionDifferenceTest, LeftEmpty)
{
    std::vector<int> v1 = {};
    std::vector<int> v2 = {        5,  7,  9,10};

    auto range1 = llvm::make_range(v1.begin(), v1.end());
    auto range2 = llvm::make_range(v2.begin(), v2.end());

    std::vector<int> intersection;
    std::vector<int> v1_minus_v2;
    std::vector<int> v2_minus_v1;

    gazer::intersection_difference(
        range1, range2, std::back_inserter(intersection),
        std::back_inserter(v1_minus_v2), std::back_inserter(v2_minus_v1)
    );

    ASSERT_EQ(intersection.size(), 0);
    ASSERT_EQ(v1_minus_v2.size(), 0);
    ASSERT_EQ(v2_minus_v1.size(), 4);

    std::vector<int> expected_v2_minus_v1 = {5, 7, 9, 10};

    ASSERT_EQ(v2_minus_v1,  expected_v2_minus_v1);
}

TEST(IntersectionDifferenceTest, BothEmpty)
{
    std::vector<int> v1 = {};
    std::vector<int> v2 = {};

    auto range1 = llvm::make_range(v1.begin(), v1.end());
    auto range2 = llvm::make_range(v2.begin(), v2.end());

    std::vector<int> intersection;
    std::vector<int> v1_minus_v2;
    std::vector<int> v2_minus_v1;

    gazer::intersection_difference(
        range1, range2, std::back_inserter(intersection),
        std::back_inserter(v1_minus_v2), std::back_inserter(v2_minus_v1)
    );

    ASSERT_EQ(intersection.size(), 0);
    ASSERT_EQ(v1_minus_v2.size(), 0);
    ASSERT_EQ(v2_minus_v1.size(), 0);
}

TEST(IntersectionDifferenceTest, Duplicates)
{
    std::vector<int> v1 = {5,1,3,1,1,4,4};
    std::vector<int> v2 = {9,4,3,2,4,2,4};

    std::vector<int> expected_intersection = {3, 4};
    std::vector<int> expected_v1_minus_v2 = {5, 1, 1, 1};
    std::vector<int> expected_v2_minus_v1 = {9, 2, 2};

    auto range1 = llvm::make_range(v1.begin(), v1.end());
    auto range2 = llvm::make_range(v2.begin(), v2.end());
  
    std::vector<int> intersection;
    std::vector<int> v1_minus_v2;
    std::vector<int> v2_minus_v1;

    gazer::intersection_difference(
        range1, range2, std::back_inserter(intersection),
        std::back_inserter(v1_minus_v2), std::back_inserter(v2_minus_v1)
    );

    ASSERT_EQ(intersection, expected_intersection);
    ASSERT_EQ(v1_minus_v2,  expected_v1_minus_v2);
    ASSERT_EQ(v2_minus_v1,  expected_v2_minus_v1);
}


}