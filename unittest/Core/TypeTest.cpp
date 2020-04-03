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
#include "gazer/Core/Type.h"

#include "gazer/Core/GazerContext.h"

#include <llvm/ADT/APFloat.h>

#include <gazer/Core/ExprTypes.h>
#include <gazer/Core/LiteralExpr.h>
#include <gtest/gtest.h>

using namespace gazer;

TEST(TypeTest, PrintType)
{
    GazerContext context;

    BoolType& boolTy = BoolType::Get(context);
    EXPECT_EQ(boolTy.getName(), "Bool");
    
    BvType& bvTy32 = BvType::Get(context, 32);
    EXPECT_EQ(bvTy32.getName(), "Bv32");

    BvType& bvTy57 = BvType::Get(context, 57);
    EXPECT_EQ(bvTy57.getName(), "Bv57");

    FloatType& halfTy = FloatType::Get(context, FloatType::Half);
    EXPECT_EQ(halfTy.getName(), "Float16");

    FloatType& floatTy = FloatType::Get(context, FloatType::Single);
    EXPECT_EQ(floatTy.getName(), "Float32");

    FloatType& doubleTy = FloatType::Get(context, FloatType::Double);
    EXPECT_EQ(doubleTy.getName(), "Float64");

    IntType& intTy = IntType::Get(context);
    EXPECT_EQ(intTy.getName(), "Int");

    RealType& realTy = RealType::Get(context);
    EXPECT_EQ(realTy.getName(), "Real");

    ArrayType& arrTy = ArrayType::Get(intTy, bvTy32);
    EXPECT_EQ(arrTy.getName(), "[Int -> Bv32]");

    TupleType& intPairTy = TupleType::Get(intTy, intTy);
    EXPECT_EQ(intPairTy.getName(), "(Int, Int)");

    TupleType& intBoolRealTy = TupleType::Get(intTy, boolTy, realTy);
    EXPECT_EQ(intBoolRealTy.getName(), "(Int, Bool, Real)");
}

TEST(TypeTest, TypeEquals)
{
    GazerContext context;

    BoolType& boolTy = BoolType::Get(context);
    BvType& bvTy32 = BvType::Get(context, 32);
    BvType& bvTy57 = BvType::Get(context, 57);
    FloatType& floatTy = FloatType::Get(context, FloatType::Single);
    FloatType& doubleTy = FloatType::Get(context, FloatType::Double);
    
    EXPECT_TRUE(boolTy == boolTy);
    EXPECT_TRUE(bvTy32 == bvTy32);
    EXPECT_TRUE(floatTy == floatTy);

    EXPECT_FALSE(boolTy == bvTy32);
    EXPECT_FALSE(bvTy32 == boolTy);
    EXPECT_FALSE(bvTy32 == bvTy57);
    EXPECT_FALSE(floatTy == doubleTy);

    GazerContext ctx2;

    BoolType& boolTy2 = BoolType::Get(ctx2);
    EXPECT_NE(boolTy, boolTy2);
}

TEST(TypeTest, CreateBvConcatExpr)
{
    GazerContext context;

    BvType& bvTy32 = BvType::Get(context, 32);
    BvType& bvTy64 = BvType::Get(context, 64);

    auto one = BvLiteralExpr::Get(bvTy32, 1);
    auto two = BvLiteralExpr::Get(bvTy32, 2);
    auto concat = BvConcatExpr::Create(one, two);

    EXPECT_EQ(concat->getType(), bvTy64);
    EXPECT_EQ(concat->getLeft(), one);
    EXPECT_EQ(concat->getRight(), two);
}

TEST(TypeTest, CreateSelectExpr)
{
    GazerContext context;
    IntType& intTy = IntType::Get(context);

    auto cond = context.createVariable("C", BoolType::Get(context))->getRefExpr();
    auto one = IntLiteralExpr::Get(intTy, 1);
    auto two = IntLiteralExpr::Get(intTy, 2);

    auto select = SelectExpr::Create(cond, one, two);

    EXPECT_EQ(select->getType(), intTy);
    EXPECT_EQ(select->getCondition(), cond);
    EXPECT_EQ(select->getThen(), one);
    EXPECT_EQ(select->getElse(), two);
}

TEST(TypeTest, FloatPrecision)
{
    GazerContext context;

    FloatType& halfTy = FloatType::Get(context, FloatType::Half);
    FloatType& floatTy = FloatType::Get(context, FloatType::Single);
    FloatType& doubleTy = FloatType::Get(context, FloatType::Double);
    FloatType& quadTy = FloatType::Get(context, FloatType::Quad);

    EXPECT_EQ(halfTy.getWidth(), 16);
    EXPECT_EQ(floatTy.getWidth(), 32);
    EXPECT_EQ(doubleTy.getWidth(), 64);
    EXPECT_EQ(quadTy.getWidth(), 128);

    EXPECT_EQ(halfTy.getExponentWidth(), 5);
    EXPECT_EQ(floatTy.getExponentWidth(), 8);
    EXPECT_EQ(doubleTy.getExponentWidth(), 11);
    EXPECT_EQ(quadTy.getExponentWidth(), 15);

    EXPECT_EQ(halfTy.getSignificandWidth(), 11);
    EXPECT_EQ(floatTy.getSignificandWidth(), 24);
    EXPECT_EQ(doubleTy.getSignificandWidth(), 53);
    EXPECT_EQ(quadTy.getSignificandWidth(), 113);

    EXPECT_EQ(&halfTy.getLLVMSemantics(), &llvm::APFloat::IEEEhalf());
    EXPECT_EQ(&floatTy.getLLVMSemantics(), &llvm::APFloat::IEEEsingle());
    EXPECT_EQ(&doubleTy.getLLVMSemantics(), &llvm::APFloat::IEEEdouble());
    EXPECT_EQ(&quadTy.getLLVMSemantics(), &llvm::APFloat::IEEEquad());
}

TEST(TypeTest, Bitvectors)
{
    GazerContext context;

    BvType& bvTy1  = BvType::Get(context, 1);
    BvType& bvTy32 = BvType::Get(context, 32);
    BvType& bvTy57 = BvType::Get(context, 57);

    EXPECT_EQ(bvTy1.getWidth(), 1);
    EXPECT_EQ(bvTy32.getWidth(), 32);
    EXPECT_EQ(bvTy57.getWidth(), 57);

    EXPECT_EQ(bvTy32, BvType::Get(context, 32));
    EXPECT_EQ(bvTy57, BvType::Get(context, 57));
}

TEST(TypeTest, Arrays)
{
    GazerContext context;
    IntType& intTy = IntType::Get(context);
    BvType& bvTy = BvType::Get(context, 32);

    ArrayType& intToBvArrTy = ArrayType::Get(intTy, bvTy);

    // Subsequent calls to ArrayType::get should return the original object
    EXPECT_EQ(intToBvArrTy, ArrayType::Get(intTy, bvTy));
    EXPECT_EQ(&intToBvArrTy, &ArrayType::Get(intTy, bvTy));

    ArrayType& bvToIntArrTy = ArrayType::Get(bvTy, intTy);
    EXPECT_NE(intToBvArrTy, bvToIntArrTy);
}

TEST(TypeTest, Tuples)
{
    GazerContext context;
    BoolType& boolTy = BoolType::Get(context);
    IntType& intTy = IntType::Get(context);
    RealType& realTy = RealType::Get(context);

    TupleType& intPairTy = TupleType::Get(intTy, intTy);
    EXPECT_EQ(intPairTy.getName(), "(Int, Int)");

    TupleType& intPair2 = TupleType::Get(intTy, intTy);
    EXPECT_TRUE(intPairTy == intPair2);

    TupleType& intBoolRealTy = TupleType::Get(intTy, boolTy, realTy);
    EXPECT_EQ(intBoolRealTy.getName(), "(Int, Bool, Real)");

    EXPECT_FALSE(intBoolRealTy == intPairTy);
}