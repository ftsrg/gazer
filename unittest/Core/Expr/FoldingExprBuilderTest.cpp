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
#include "gazer/Core/Expr/ExprBuilder.h"

#include <llvm/Support/raw_ostream.h>

#include <gtest/gtest.h>

using namespace gazer;

namespace
{

class FoldingExprBuilderTest : public ::testing::Test
{
protected:
    GazerContext context;
    std::unique_ptr<ExprBuilder> builder;
public:
    ExprRef<IntLiteralExpr> iZero, iOne, iTwo;
    ExprRef<BvLiteralExpr> bvZero, bvOne, bvTwo, bvAllOnes;

    ExprRef<VarRefExpr> iVar, bvVar;

    FoldingExprBuilderTest()
        : builder(CreateFoldingExprBuilder(context))
    {
        iZero = builder->IntLit(0);
        iOne = builder->IntLit(1);
        iTwo = builder->IntLit(2);
        bvZero = builder->BvLit32(0);
        bvOne = builder->BvLit32(1);
        bvTwo = builder->BvLit32(2);
        bvAllOnes = builder->BvLit(llvm::APInt::getAllOnesValue(32));
        iVar = context.createVariable("A", IntType::Get(context))->getRefExpr();
        bvVar = context.createVariable("B", BvType::Get(context, 32))->getRefExpr();
    }
};

}

TEST_F(FoldingExprBuilderTest, TestBinaryArithmetic)
{
    // 1 + 1 == 2
    EXPECT_EQ(iTwo, builder->Add(iOne, iOne));
    EXPECT_EQ(bvTwo, builder->Add(bvOne, bvOne));
    
    // 1 + 0 == 1
    // 0 + 1 == 1
    EXPECT_EQ(iOne,  builder->Add(iOne,   iZero));
    EXPECT_EQ(iOne,  builder->Add(iZero,  iOne));
    EXPECT_EQ(bvOne, builder->Add(bvOne,  bvZero));
    EXPECT_EQ(bvOne, builder->Add(bvZero, bvOne));
    
    // A + 0 == A
    // 0 + A == A
    EXPECT_EQ(iVar,  builder->Add(iVar,   iZero));
    EXPECT_EQ(iVar,  builder->Add(iZero,  iVar));
    EXPECT_EQ(bvVar, builder->Add(bvVar,  bvZero));
    EXPECT_EQ(bvVar, builder->Add(bvZero, bvVar));

    // 1 - 1 == 0
    EXPECT_EQ(iZero,  builder->Sub(iOne,  iOne));
    EXPECT_EQ(bvZero, builder->Sub(bvOne, bvOne));

    // 2 - 1 == 1
    EXPECT_EQ(iOne,  builder->Sub(iTwo,  iOne));
    EXPECT_EQ(bvOne, builder->Sub(bvTwo, bvOne));

    // 1 - 2 == -1
    EXPECT_EQ(builder->IntLit(-1), builder->Sub(iOne,  iTwo));
    EXPECT_EQ(bvAllOnes,           builder->Sub(bvOne, bvTwo));

    // A - 0 == A
    // 0 - A != A
    EXPECT_EQ(iVar,  builder->Sub(iVar,   iZero));
    EXPECT_NE(iVar,  builder->Sub(iZero,  iVar));
    EXPECT_EQ(bvVar, builder->Sub(bvVar,  bvZero));
    EXPECT_NE(bvVar, builder->Sub(bvZero, bvVar));

    // A * 0 == 0
    // 0 * A == 0
    EXPECT_EQ(iZero, builder->Mul(iVar, iZero));
    EXPECT_EQ(iZero, builder->Mul(iZero, iVar));
}


TEST_F(FoldingExprBuilderTest, TestBvOverflow)
{
    auto smin = builder->BvLit(llvm::APInt::getSignedMinValue(32));
    auto smax = builder->BvLit(llvm::APInt::getSignedMaxValue(32));

    // INT_MAX + 1 == INT_MIN
    EXPECT_EQ(smin, builder->Add(smax, bvOne));

    // INT_MIN - 1 == INT_MAX
    EXPECT_EQ(smax, builder->Sub(smin, bvOne));

    // INT_MIN div (-1) == INT_MIN
    EXPECT_EQ(smin, builder->BvSDiv(smin, bvAllOnes));
}