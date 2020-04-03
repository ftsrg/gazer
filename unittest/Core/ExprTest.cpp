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

TEST(Expr, CanCreateMultipleExpressions)
{
    GazerContext context;
    auto x = context.createVariable("X", IntType::Get(context))->getRefExpr();

    std::vector<ExprPtr> exprs;
    for (unsigned i = 0; i < 10000; ++i) {
        // Force the ExprStorage hash table to rehash
        exprs.push_back(EqExpr::Create(x, IntLiteralExpr::Get(context, i)));
    }
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

TEST(Expr, CanCreateFloatExpressions)
{
    GazerContext context;
    auto& fp32Ty = FloatType::Get(context, FloatType::Single);
    auto& fp64Ty = FloatType::Get(context, FloatType::Double);

    auto x = context.createVariable("X", fp32Ty)->getRefExpr();
    auto y = context.createVariable("Y", fp32Ty)->getRefExpr();

    auto add = FAddExpr::Create(x, y, llvm::APFloat::rmTowardZero);

    EXPECT_EQ(add->getType(), fp32Ty);
    EXPECT_EQ(add->getOperand(0), x);
    EXPECT_EQ(add->getOperand(1), y);
    EXPECT_EQ(add->getRoundingMode(), llvm::APFloat::rmTowardZero);
    EXPECT_EQ(add->getKind(), Expr::FAdd);

    auto cmp = FEqExpr::Create(add, x);
    EXPECT_EQ(cmp->getType(), BoolType::Get(context));
    EXPECT_EQ(cmp->getOperand(0), add);
    EXPECT_EQ(cmp->getOperand(1), x);

    auto fext = FCastExpr::Create(add, fp64Ty, llvm::APFloat::rmNearestTiesToAway);
    EXPECT_EQ(fext->getType(), fp64Ty);
    EXPECT_EQ(fext->getOperand(0), add);
    EXPECT_EQ(fext->getRoundingMode(), llvm::APFloat::rmNearestTiesToAway);

    auto fIsNaN = FIsNanExpr::Create(fext);
    EXPECT_EQ(fIsNaN->getType(), BoolType::Get(context));
    EXPECT_EQ(fIsNaN->getOperand(0), fext);

    EXPECT_EQ(
        FpToSignedExpr::Create(fext, BvType::Get(context, 64), llvm::APFloat::rmNearestTiesToAway)->getType(),
        BvType::Get(context, 64)
    );
    EXPECT_EQ(
        FpToUnsignedExpr::Create(fext, BvType::Get(context, 32), llvm::APFloat::rmNearestTiesToAway)->getType(),
        BvType::Get(context, 32)
    );

    auto one = BvLiteralExpr::Get(BvType::Get(context, 32), 1);
    EXPECT_EQ(
        SignedToFpExpr::Create(one, fp32Ty, llvm::APFloat::rmNearestTiesToAway)->getType(),
        fp32Ty);
    EXPECT_EQ(
        UnsignedToFpExpr::Create(one, fp64Ty, llvm::APFloat::rmNearestTiesToAway)->getType(),
        fp64Ty);
}

TEST(Expr, CanCreateArrayExpressions)
{
    GazerContext context;
    auto& intTy = IntType::Get(context);
    auto& arrTy = ArrayType::Get(intTy, intTy);

    auto array = context.createVariable("array", arrTy)->getRefExpr();
    auto zero = IntLiteralExpr::Get(intTy, 0);
    auto one = IntLiteralExpr::Get(intTy, 1);

    auto write = ArrayWriteExpr::Create(array, zero, one);
    EXPECT_EQ(write->getType(), arrTy);
    EXPECT_EQ(write->getOperand(0), array);
    EXPECT_EQ(write->getOperand(1), zero);
    EXPECT_EQ(write->getOperand(2), one);

    auto read = ArrayReadExpr::Create(array, zero);
    EXPECT_EQ(read->getType(), intTy);
    EXPECT_EQ(read->getOperand(0), array);
    EXPECT_EQ(read->getOperand(1), zero);
}

TEST(Expr, CanCreateTupleExpressions)
{
    GazerContext context;
    auto& intTy = IntType::Get(context);
    auto& bvTy = BvType::Get(context, 32);
    auto& tupTy = TupleType::Get(intTy, bvTy);

    auto tuple = context.createVariable("tuple", tupTy)->getRefExpr();
    auto iZero = IntLiteralExpr::Get(intTy, 0);
    auto bvOne = BvLiteralExpr::Get(bvTy, 1);

    auto construct = TupleConstructExpr::Create(tupTy, iZero, bvOne);
    EXPECT_EQ(construct->getType(), tupTy);
    EXPECT_EQ(construct->getOperand(0), iZero);
    EXPECT_EQ(construct->getOperand(1), bvOne);

    auto read = TupleSelectExpr::Create(construct, 0);
    EXPECT_EQ(read->getType(), intTy);

    read = TupleSelectExpr::Create(construct, 1);
    EXPECT_EQ(read->getType(), bvTy);
}
