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
#include "gazer/Core/Expr.h"
#include "gazer/Core/GazerContext.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <gtest/gtest.h>

using namespace gazer;

TEST(Expr, CanCreateExpressions)
{
    GazerContext context;

    auto x = context.createVariable("X", BvType::Get(context, 32))->getRefExpr();
    auto y = context.createVariable("Y", BvType::Get(context, 32))->getRefExpr();

    auto e1 = EqExpr::Create(x, y);
    auto e2 = EqExpr::Create(x, y);

    ASSERT_EQ(e1, e2);
}

TEST(Expr, CanCreateLiteralExpressions)
{
    GazerContext context;

    auto t = BoolLiteralExpr::True(context);
    auto f = BoolLiteralExpr::False(context);

    EXPECT_EQ(t->getValue(), true);
    EXPECT_EQ(f->getValue(), false);

    t = BoolLiteralExpr::Get(context, true);
    f = BoolLiteralExpr::Get(context, false);

    EXPECT_EQ(t->getValue(), true);
    EXPECT_EQ(f->getValue(), false);

    auto zero = BvLiteralExpr::Get(BvType::Get(context, 32), llvm::APInt{32, 0});
    auto one  = BvLiteralExpr::Get(BvType::Get(context, 32), llvm::APInt{32, 1});

    EXPECT_EQ(zero->getValue(), llvm::APInt(32, 0));
    EXPECT_EQ(one->getValue(), llvm::APInt(32, 1));

    auto iZero = IntLiteralExpr::Get(IntType::Get(context), 0);
    auto iOne = IntLiteralExpr::Get(IntType::Get(context), 1);

    EXPECT_EQ(iZero->getValue(), 0);
    EXPECT_EQ(iOne->getValue(), 1);

    auto rZero = RealLiteralExpr::Get(RealType::Get(context), 0);
    auto rOne = RealLiteralExpr::Get(RealType::Get(context), 1);
    auto rHalf = RealLiteralExpr::Get(RealType::Get(context), boost::rational<long long int>{1, 2});

    EXPECT_EQ(rZero->getValue(), 0);
    EXPECT_EQ(rOne->getValue(), 1);
    EXPECT_EQ(rHalf->getValue(), boost::rational<long long int>(1, 2));
}

TEST(Expr, CanFormExpressionDAG)
{
    GazerContext context;

    auto eq = EqExpr::Create(
        BvLiteralExpr::Get(BvType::Get(context, 32), llvm::APInt{32, 0}),
        ZExtExpr::Create(
            context.createVariable("a", BvType::Get(context, 8))->getRefExpr(),
            BvType::Get(context, 32)
        )
    );

    auto expr = ImplyExpr::Create(
        AndExpr::Create(
            BoolLiteralExpr::True(BoolType::Get(context)),
            eq
        ),
        OrExpr::Create(
            eq,
            BoolLiteralExpr::False(BoolType::Get(context))
        )
    );
}
