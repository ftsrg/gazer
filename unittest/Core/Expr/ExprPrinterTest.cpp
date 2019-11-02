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
#include "gazer/Core/Expr/ExprUtils.h"
#include "gazer/Core/Expr/ExprBuilder.h"

#include <llvm/Support/raw_ostream.h>

#include <gtest/gtest.h>

using namespace gazer;

namespace
{

::testing::AssertionResult printEquals(const std::string& expected, const ExprPtr& expr, unsigned radix = 10)
{
    std::string buff;
    llvm::raw_string_ostream rso{buff};

    InfixPrintExpr(expr, rso, radix);
    rso.flush();

    if (expected == buff) {
        return ::testing::AssertionSuccess();
    }

    return ::testing::AssertionFailure()
        << "Printed expression '" << buff
        << "' does not match with expected '" << expected << "'.";
}

}

TEST(InfixPrintExpr, TestPrintBoolLiteral)
{
    GazerContext ctx;

    EXPECT_TRUE(printEquals("true", BoolLiteralExpr::True(ctx)));
    EXPECT_TRUE(printEquals("false", BoolLiteralExpr::False(ctx)));
}

TEST(InfixPrintExpr, TestPrintBvLiteral)
{
    GazerContext ctx;
    auto builder = CreateExprBuilder(ctx);

    EXPECT_TRUE(printEquals("0bv8",     builder->BvLit(0, 8),  10));
    EXPECT_TRUE(printEquals("2#0bv8",   builder->BvLit(0, 8),  2));
    EXPECT_TRUE(printEquals("8#0bv8",   builder->BvLit(0, 8),  8));
    EXPECT_TRUE(printEquals("16#0bv8",  builder->BvLit(0, 8),  16));
}

TEST(InfixPrintExpr, TestPrintIntLiteral)
{
    GazerContext ctx;
    auto builder = CreateExprBuilder(ctx);

    EXPECT_TRUE(printEquals("0",     builder->IntLit(0)));
    EXPECT_TRUE(printEquals("1",     builder->IntLit(1)));
    EXPECT_TRUE(printEquals("65535", builder->IntLit(65535)));
}

TEST(InfixPrintExpr, TestPrintBvCast)
{
    GazerContext ctx;
    auto builder = CreateExprBuilder(ctx);

    auto e1 = builder->ZExt(builder->BvLit(12, 8), BvType::Get(ctx, 32));
    auto e2 = builder->SExt(builder->BvLit(0, 16), BvType::Get(ctx, 32));
    auto e3 = builder->Extract(builder->BvLit(255, 32), 0, 8);
    auto e4 = builder->Extract(builder->BvLit(255, 32), 8, 8);

    EXPECT_TRUE(printEquals("zext.bv8.bv32(12bv8)", e1));
    EXPECT_TRUE(printEquals("sext.bv16.bv32(0bv16)", e2));
    EXPECT_TRUE(printEquals("extract.bv32.bv8(255bv32, 0, 8)", e3));
    EXPECT_TRUE(printEquals("extract.bv32.bv8(255bv32, 8, 8)", e4));
}

TEST(InfixPrintExpr, TestPrintAnd)
{
    GazerContext ctx;
    auto builder = CreateExprBuilder(ctx);

    auto e1 = builder->And(builder->True(), builder->False());
    auto e2 = builder->And(builder->Not(builder->True()), builder->False());

    EXPECT_TRUE(printEquals("true and false", e1));
    EXPECT_TRUE(printEquals("(not true) and false", e2));
}

TEST(InfixPrintExpr, TestPrintOr)
{
    GazerContext ctx;
    auto builder = CreateExprBuilder(ctx);

    auto e1 = builder->Or(builder->True(), builder->False());
    auto e2 = builder->Or(builder->Not(builder->True()), builder->False());

    EXPECT_TRUE(printEquals("true or false", e1));
    EXPECT_TRUE(printEquals("(not true) or false", e2));
}

TEST(InfixPrintExpr, TestPrintNot)
{
    GazerContext ctx;
    auto builder = CreateExprBuilder(ctx);

    auto e1 = builder->Not(builder->True());
    auto e2 = builder->Not(builder->And(builder->True(), builder->False()));

    EXPECT_TRUE(printEquals("not true", e1));
    EXPECT_TRUE(printEquals("not (true and false)", e2));
}
