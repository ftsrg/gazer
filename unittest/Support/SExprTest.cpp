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
#include "gazer/Support/SExpr.h"

#include <llvm/Support/raw_ostream.h>
#include <gtest/gtest.h>

using namespace gazer;

TEST(SExprTest, TestParse)
{
    std::unique_ptr<sexpr::Value> expected;

    expected.reset(sexpr::list({
        sexpr::atom("A"),
        sexpr::atom("B"),
        sexpr::atom("C")
    }));
    EXPECT_EQ(*sexpr::parse("(A B\n \nC)"), *expected);

    expected.reset(sexpr::list({
        sexpr::atom("A"),
        sexpr::list({
            sexpr::atom("X"),
            sexpr::atom("Y"),
            sexpr::atom("Z")
        })
    }));
    EXPECT_EQ(*sexpr::parse("(A (X Y Z))"), *expected);

    expected.reset(sexpr::list({
        sexpr::atom("A"),
        sexpr::list({
            sexpr::atom("X"),
            sexpr::atom("Y"),
            sexpr::list({ sexpr::atom("Z") })
        })
    }));
    EXPECT_EQ(*sexpr::parse("(A (X Y (Z)))"), *expected);
}