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
#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <gtest/gtest.h>

using namespace gazer;

TEST(SolverZ3Test, SmokeTest1)
{
    GazerContext ctx;
    Z3SolverFactory factory;
    auto solver = factory.createSolver(ctx);

    auto a = ctx.createVariable("A", BoolType::Get(ctx));
    auto b = ctx.createVariable("B", BoolType::Get(ctx));

    // (A & B)
    solver->add(AndExpr::Create(
        a->getRefExpr(),
        b->getRefExpr()
    ));

    auto result = solver->run();

    ASSERT_EQ(result, Solver::SAT);
    auto model = solver->getModel();

    ASSERT_EQ(model.eval(a->getRefExpr()), BoolLiteralExpr::True(ctx));
    ASSERT_EQ(model.eval(a->getRefExpr()), BoolLiteralExpr::True(ctx));
}

TEST(SolverZ3Test, FpaWithRoundingMode)
{
    GazerContext ctx;
    Z3SolverFactory factory;
    auto solver = factory.createSolver(ctx);

    // fcast.fp64(tmp) == 0
    auto tmp = ctx.createVariable("tmp", FloatType::Get(ctx, FloatType::Single));
    auto fcast = FCastExpr::Create(tmp->getRefExpr(), FloatType::Get(ctx, FloatType::Double), llvm::APFloatBase::rmNearestTiesToEven);
    auto eq = FEqExpr::Create(fcast, FloatLiteralExpr::Get(FloatType::Get(ctx, FloatType::Double), llvm::APFloat{0.0}));

    solver->add(eq);

    auto result = solver->run();
    ASSERT_EQ(result, Solver::SAT);
    auto model = solver->getModel();

    ASSERT_EQ(
        model.eval(tmp->getRefExpr()),
        FloatLiteralExpr::Get(FloatType::Get(ctx, FloatType::Single), llvm::APFloat(0.0f))
    );
}

TEST(SolverZ3Test, Arrays)
{
    GazerContext ctx;
    Z3SolverFactory factory;
    auto solver = factory.createSolver(ctx);

    // Example from the Z3 tutorial:
    //  (declare-const x Int)
    //  (declare-const y Int)
    //  (declare-const a1 (Array Int Int))
    //  (assert (= (select a1 x) x))
    //  (assert (= (store a1 x y) a1))
    auto x = ctx.createVariable("x", IntType::Get(ctx));
    auto y = ctx.createVariable("y", IntType::Get(ctx));
    auto a1 = ctx.createVariable("a1", ArrayType::Get(IntType::Get(ctx), IntType::Get(ctx)));

    solver->add(EqExpr::Create(
        ArrayReadExpr::Create(a1->getRefExpr(), x->getRefExpr()),
        x->getRefExpr()
    ));
    solver->add(EqExpr::Create(
        ArrayWriteExpr::Create(a1->getRefExpr(), x->getRefExpr(), y->getRefExpr()),
        a1->getRefExpr()
    ));

    auto result = solver->run();
    ASSERT_EQ(result, Solver::SAT);

    solver->add(NotEqExpr::Create(x->getRefExpr(), y->getRefExpr()));
    result = solver->run();
    ASSERT_EQ(result, Solver::UNSAT);
}

TEST(SolverZ3Test, ArrayLiterals)
{
    GazerContext ctx;
    Z3SolverFactory factory;
    auto solver = factory.createSolver(ctx);

    // Array literal with a default value.
    auto a1 = ArrayLiteralExpr::Get(
        ArrayType::Get(IntType::Get(ctx), IntType::Get(ctx)),
        {
            { IntLiteralExpr::Get(ctx, 1), IntLiteralExpr::Get(ctx, 1) },
            { IntLiteralExpr::Get(ctx, 2), IntLiteralExpr::Get(ctx, 2) }
        },
        IntLiteralExpr::Get(ctx, 0)
    );

    // Array literal without a default value
    auto a2 = ArrayLiteralExpr::Get(
        ArrayType::Get(IntType::Get(ctx), IntType::Get(ctx)),
        {
            { IntLiteralExpr::Get(ctx, 1), IntLiteralExpr::Get(ctx, 1) },
            { IntLiteralExpr::Get(ctx, 2), IntLiteralExpr::Get(ctx, 2) }
        }
    );

    solver->add(EqExpr::Create(a1, a2));
    auto result = solver->run();
    ASSERT_EQ(result, Solver::SAT);
}

TEST(SolverZ3Test, Tuples)
{
    GazerContext ctx;
    Z3SolverFactory factory;
    auto solver = factory.createSolver(ctx);

    // Example from the Z3 tutorial:
    //  (declare-const p1 (Pair Int Int))
    //  (declare-const p2 (Pair Int Int))
    //  (assert (= p1 p2))
    //  (assert (> (second p1) 20))
    //  (check-sat)
    //  (assert (not (= (first p1) (first p2))))
    //  (check-sat)
    auto& intTy = IntType::Get(ctx);
    auto& tupTy = TupleType::Get(intTy, intTy);

    auto p1 = ctx.createVariable("p1", tupTy)->getRefExpr();
    auto p2 = ctx.createVariable("p2", tupTy)->getRefExpr();

    solver->add(EqExpr::Create(p1, p2));
    solver->add(GtExpr::Create(TupleSelectExpr::Create(p1, 1), IntLiteralExpr::Get(ctx, 20)));

    auto status = solver->run();
    EXPECT_EQ(status, Solver::SAT);

    solver->add(NotEqExpr::Create(TupleSelectExpr::Create(p1, 0), TupleSelectExpr::Create(p2, 0)));

    status = solver->run();
    EXPECT_EQ(status, Solver::UNSAT);
}