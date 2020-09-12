//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2020 Contributors to the Gazer project
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

#include <gtest/gtest.h>

using namespace gazer;

TEST(ExprBuilderTest, TupleConstruct)
{
    GazerContext ctx;
    auto builder = CreateExprBuilder(ctx);

    auto tuple = builder->Tuple(
        builder->IntLit(1),
        builder->BvLit8(255),
        builder->True()
    );

    ASSERT_EQ(tuple->getKind(), Expr::TupleConstruct);
    ASSERT_TRUE(tuple->getType().isTupleType());

    auto& tupTy = llvm::cast<TupleType>(tuple->getType());
    ASSERT_EQ(tupTy.getNumSubtypes(), 3);
    EXPECT_EQ(tupTy.getTypeAtIndex(0), IntType::Get(ctx));
    EXPECT_EQ(tupTy.getTypeAtIndex(1), BvType::Get(ctx, 8));
    EXPECT_EQ(tupTy.getTypeAtIndex(2), BoolType::Get(ctx));
}

TEST(ExprBuilderTest, TupleInsertionsInTupleConstruct)
{
    GazerContext ctx;
    auto builder = CreateExprBuilder(ctx);

    auto tuple = builder->Tuple(
        builder->IntLit(1),
        builder->BvLit8(255),
        builder->True()
    );

    auto newTuple = builder->TupleInsert(tuple, builder->BvLit8(0), 1);
    ASSERT_EQ(newTuple->getType(), tuple->getType());
    EXPECT_EQ(newTuple, builder->Tuple(
       builder->IntLit(1),
       builder->BvLit8(0),
       builder->True()
    ));
}

TEST(ExprBuilderTest, TupleInsertionsInOther)
{
    GazerContext ctx;
    auto builder = CreateExprBuilder(ctx);

    auto& tupTy = TupleType::Get(IntType::Get(ctx), BvType::Get(ctx, 8), BoolType::Get(ctx));
    Variable* foo = ctx.createVariable("foo", tupTy);
    auto tuple = foo->getRefExpr();

    auto newTuple = builder->TupleInsert(tuple, builder->BvLit8(0), 1);
    ASSERT_EQ(newTuple, builder->Tuple(
        builder->TupSel(tuple, 0),
        builder->BvLit8(0),
        builder->TupSel(tuple, 2)
    ));
}

