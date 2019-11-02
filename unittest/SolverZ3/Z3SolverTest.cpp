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

TEST(SolverZ3Test, TestFpaWithRoundingMode)
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
}
