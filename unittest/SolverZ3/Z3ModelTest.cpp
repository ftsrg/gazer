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
#include "gazer/Core/Solver/Model.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <gtest/gtest.h>

using namespace gazer;

namespace
{

TEST(Z3ModelTest, Arrays)
{
    GazerContext ctx;
    Z3SolverFactory factory;

    auto one = IntLiteralExpr::Get(ctx, 1);
    auto array = ctx.createVariable("array", ArrayType::Get(IntType::Get(ctx), IntType::Get(ctx)));
    auto write = ArrayWriteExpr::Create(array->getRefExpr(), one, one);

    auto solver = factory.createSolver(ctx);
    solver->add(EqExpr::Create(ArrayReadExpr::Create(write, one), one));

    auto status = solver->run();

    ASSERT_EQ(status, Solver::SAT);
    ASSERT_EQ(solver->getModel()->evaluate(ArrayReadExpr::Create(write, one)), one);
}

} // namespace
