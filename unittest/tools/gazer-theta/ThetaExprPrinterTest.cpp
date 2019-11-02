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
#include "../../../tools/gazer-theta/lib/ThetaCfaGenerator.h"

#include "gazer/Core/Expr/ExprBuilder.h"

#include <gtest/gtest.h>

using namespace gazer;

namespace
{

class ThetaExprPrinterTest : public ::testing::Test
{
public:
    ThetaExprPrinterTest();

protected:
    GazerContext ctx;
    std::unique_ptr<ExprBuilder> b;
    std::vector<std::pair<ExprPtr, std::string>> tests;
};

ThetaExprPrinterTest::ThetaExprPrinterTest()
    : b(CreateExprBuilder(ctx)),
    tests({
              { b->True(),                                    "true" },
              { b->False(),                                   "false" },
              { b->IntLit(1),                                 "1" },
              { b->Add(b->IntLit(2), b->IntLit(3)),           "(2 + 3)" },
              { b->Sub(b->IntLit(2), b->IntLit(3)),           "(2 - 3)" },
              { b->Mul(b->IntLit(2), b->IntLit(3)),           "(2 * 3)" },
              { b->Div(b->IntLit(2), b->IntLit(3)),           "(2 / 3)" },
              { b->Add(b->IntLit(-2), b->IntLit(3)),          "((-2) + 3)" },
              { b->Sub(b->IntLit(2), b->IntLit(-3)),          "(2 - (-3))" },
              { b->And(b->True(), b->False()),                "(true and false)" },
              { b->Or(b->False(), b->True()),                 "(false or true)" },
              { b->And({b->True(), b->True(), b->False()}),   "(true and true and false)" },
              { b->Or({b->True(), b->True(), b->False()}),    "(true or true or false)" },
              { b->Mul(b->Mul(b->IntLit(1), b->Div(b->IntLit(2), b->IntLit(3))), b->IntLit(4)), "((1 * (2 / 3)) * 4)"},
              { b->And({
                           b->Eq(b->IntLit(1), b->IntLit(1)),
                           b->Lt(b->IntLit(1), b->IntLit(2)),
                           b->Gt(b->IntLit(2), b->IntLit(1)) }),       "((1 = 1) and (1 < 2) and (2 > 1))"
              },
              { b->Select(b->Eq(b->IntLit(1), b->IntLit(2)), b->IntLit(3), b->IntLit(4)), "(if (1 = 2) then 3 else 4)" }
          })
{}

TEST_F(ThetaExprPrinterTest, TestPrintExpr)
{
    for (auto& [ expr, expected ] : tests) {
        EXPECT_EQ(theta::printThetaExpr(expr), expected);
    }
}

}
