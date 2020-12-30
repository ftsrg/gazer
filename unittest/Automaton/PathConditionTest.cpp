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
#include "gazer/Automaton/CfaUtils.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Expr/ExprBuilder.h"

#include <llvm/Support/raw_ostream.h>

#include <gtest/gtest.h>

using namespace gazer;

namespace
{

TEST(PathConditionTest, PredecessorTest)
{
    GazerContext ctx;
    AutomataSystem system(ctx);

    Cfa* cfa = system.createCfa("main");
    auto x = cfa->createLocal("x", IntType::Get(ctx));
    auto y = cfa->createLocal("y", IntType::Get(ctx));
    auto z = cfa->createLocal("z", IntType::Get(ctx));

    // l0, l1 are reserved for entry and exit
    auto l2 = cfa->createLocation();
    auto l3 = cfa->createLocation();
    auto l4 = cfa->createLocation();
    auto l5 = cfa->createLocation();
    auto le = cfa->createErrorLocation();

    // l0 --> l2: { x := undef, y := undef }
    cfa->createAssignTransition(cfa->getEntry(), l2, {
        { x, UndefExpr::Get(IntType::Get(ctx)) },
        { y, UndefExpr::Get(IntType::Get(ctx)) }
    });

    auto lteq = LtEqExpr::Create(y->getRefExpr(), IntLiteralExpr::Get(ctx, 0));

    // l2 --> l3 [ y < 0 ] { z := x + 1}
    cfa->createAssignTransition(l2, l3, lteq, {
        { z, AddExpr::Create(x->getRefExpr(), IntLiteralExpr::Get(ctx, 1)) }
    });

    // l2 --> l4 [ not y < 0 ] { z := x - 1 }
    cfa->createAssignTransition(l2, l4, NotExpr::Create(lteq), {
        { z, SubExpr::Create(x->getRefExpr(), IntLiteralExpr::Get(ctx, 1)) }
    });

    // l3 --> l5 {}
    // l4 --> l5 {}
    cfa->createAssignTransition(l3, l5);
    cfa->createAssignTransition(l4, l5);

    auto eq = EqExpr::Create(z->getRefExpr(), IntLiteralExpr::Get(ctx, 0));

    // l5 --> ERROR [ z == 0 ]
    cfa->createAssignTransition(l5, le, eq);
    cfa->createAssignTransition(l5, cfa->getExit(), NotExpr::Create(eq));

    auto builder = CreateFoldingExprBuilder(ctx);

    CfaTopoSort topo(*cfa);
    PathConditionCalculator pathCond(
        topo, *builder,
        [&ctx](auto t) { return BoolLiteralExpr::True(ctx); },
        nullptr
    );

    auto expected = builder->And(
        builder->Or(
            builder->And(lteq, builder->Eq(z->getRefExpr(), builder->Add(x->getRefExpr(), builder->IntLit(1)))), // l2 --> l3
            builder->And(builder->Not(lteq), builder->Eq(z->getRefExpr(), builder->Sub(x->getRefExpr(), builder->IntLit(1)))) // l2 --> l4
        ), // l5
        builder->Eq(z->getRefExpr(), builder->IntLit(0)) // l5 --> le
    );

    auto actual = pathCond.encode(cfa->getEntry(), le);

    ASSERT_EQ(expected, actual);
}


TEST(PathConditionTest, TestParallelEdges)
{
    GazerContext ctx;
    AutomataSystem system(ctx);

    Cfa* cfa = system.createCfa("main");
    auto x = cfa->createLocal("x", IntType::Get(ctx));

    // l0, l1 are reserved for entry and exit
    auto l2 = cfa->createLocation();
    auto le = cfa->createErrorLocation();

    // l0 --> l2 { x := 1 }
    // l0 --> l2 { x := 1 }
    cfa->createAssignTransition(cfa->getEntry(), l2, {
        { x, IntLiteralExpr::Get(ctx, 1) }
    });
    cfa->createAssignTransition(cfa->getEntry(), l2, {
        { x, IntLiteralExpr::Get(ctx, 1) }
    });

    // l2 --> le
    cfa->createAssignTransition(l2, le);

    // le --> exit [False]
    cfa->createAssignTransition(le, cfa->getExit(), BoolLiteralExpr::False(ctx));

    CfaTopoSort topo(*cfa);
    auto builder = CreateFoldingExprBuilder(ctx);

    PathConditionCalculator pathCond(
        topo, *builder,
        [&ctx](auto t) { return BoolLiteralExpr::True(ctx); },
        nullptr
    );

    auto expected = builder->Or({
        builder->Eq(x->getRefExpr(), builder->IntLit(1)),
        builder->Eq(x->getRefExpr(), builder->IntLit(1)),
    });

    auto actual = pathCond.encode(cfa->getEntry(), le);
    ASSERT_EQ(expected, actual);
}

}
