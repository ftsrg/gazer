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
#include "gazer/Automaton/Cfa.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/Support/raw_ostream.h>

#include <gtest/gtest.h>

using namespace gazer;

TEST(CfaPrinter, TestPrint)
{
    GazerContext ctx;
    AutomataSystem system{ctx};

    auto calc = system.createCfa("calc");
    calc->createInput("a", BvType::Get(ctx, 32));

    auto q = calc->createLocal("q", BvType::Get(ctx, 32));
    calc->addOutput(q);

    auto cfa = system.createCfa("main");
    auto x = cfa->createInput("x", BvType::Get(ctx, 32));
    auto y = cfa->createInput("y", BvType::Get(ctx, 32));

    auto ret = cfa->createLocal("RET_VAL", BvType::Get(ctx, 32));
    cfa->addOutput(ret);

    auto m1 = cfa->createLocal("m1", BvType::Get(ctx, 32));
    auto m2 = cfa->createLocal("m2", BvType::Get(ctx, 32));

    auto l1 = cfa->createLocation();
    auto l2 = cfa->createLocation();
    auto l3 = cfa->createLocation();

    cfa->createAssignTransition(cfa->getEntry(), l1);
    cfa->createAssignTransition(l1, l2, BoolLiteralExpr::True(ctx), {
        VariableAssignment{
            m1, AddExpr::Create(x->getRefExpr(), y->getRefExpr())
        }
    });
    cfa->createCallTransition(l2, l3, calc, { m1->getRefExpr() }, {
        { m2, q->getRefExpr() }
    });

    cfa->print(llvm::errs());
}
