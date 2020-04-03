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

#include <gtest/gtest.h>

using namespace gazer;

TEST(Variable, CanCreateVariables)
{
    GazerContext context;
    Variable* x = context.createVariable("x", BvType::Get(context, 32));
    Variable* y = context.createVariable("y", BoolType::Get(context));

    ASSERT_EQ(x->getName(), "x");
    ASSERT_EQ(y->getName(), "y");

    ASSERT_EQ(x->getType(), BvType::Get(context, 32));
    ASSERT_EQ(y->getType(), BoolType::Get(context));

    EXPECT_EQ(x, context.getVariable("x"));
    context.removeVariable(x);
    EXPECT_EQ(nullptr, context.getVariable("x"));
}
