//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2021 Contributors to the Gazer project
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
#include "gazer/Verifier/BoundedModelChecker.h"

#include "gazer/Automaton/Cfa.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Z3Solver/Z3Solver.h"

#include <llvm/Support/raw_ostream.h>

#include <gtest/gtest.h>

using namespace gazer;

namespace {

class TraceBuilderImpl : public CfaTraceBuilder
{
public:
    std::unique_ptr<Trace> build(
        std::vector<Location*>& states,
        std::vector<std::vector<VariableAssignment>>& actions) override
    {
        this->capturedStates = states;
        this->capturedActions = actions;

        return nullptr;
    }

    std::vector<Location*> capturedStates;
    std::vector<std::vector<VariableAssignment>> capturedActions;
};

class BmcTest : public ::testing::Test
{
public:
    void SetUp() override
    {
        system = std::make_unique<AutomataSystem>(ctx);
        builder = CreateExprBuilder(ctx);
        solverFactory = std::make_unique<Z3SolverFactory>();
    }

    std::unique_ptr<BoundedModelChecker> createBmcEngine(unsigned bound = 0) const
    {
        BmcSettings settings{};
        settings.maxBound = bound;
        settings.trace = true;

        return std::make_unique<BoundedModelChecker>(*solverFactory, settings);
    }

protected:
    Cfa* createCounterCfa(int count);

protected:
    GazerContext ctx;
    std::unique_ptr<AutomataSystem> system;
    std::unique_ptr<ExprBuilder> builder;
    std::unique_ptr<SolverFactory> solverFactory;
};

TEST_F(BmcTest, SimpleFail)
{
    Cfa* foo = system->createCfa("foo");
    Variable* x = foo->createLocal("x", IntType::Get(ctx));

    auto* l1 = foo->createLocation();
    auto* le = foo->createErrorLocation();
    foo->addErrorCode(le, builder->IntLit(1));

    foo->createAssignTransition(foo->getEntry(), l1, {{x, builder->IntLit(0)}});
    foo->createAssignTransition(l1, le, builder->Eq(x->getRefExpr(), builder->IntLit(0)));
    foo->createAssignTransition(
        l1, foo->getExit(), builder->NotEq(x->getRefExpr(), builder->IntLit(0)));

    system->setMainAutomaton(foo);

    auto bmc = createBmcEngine(1);
    TraceBuilderImpl traceBuilder;

    auto result = bmc->check(*system, traceBuilder);

    ASSERT_TRUE(result->isFail());
    ASSERT_EQ(llvm::cast<FailResult>(*result).getErrorID(), 1);

    std::vector<Location*> expectedTrace = {
        foo->getEntry(), l1, le
    };

    ASSERT_EQ(expectedTrace, traceBuilder.capturedStates);
}

TEST_F(BmcTest, FailStraightPath)
{
    Cfa* foo = system->createCfa("foo");

    auto* l1 = foo->createLocation();
    auto* le = foo->createErrorLocation();
    foo->addErrorCode(le, builder->IntLit(1));

    foo->createAssignTransition(foo->getEntry(), l1);
    foo->createAssignTransition(l1, le);
    foo->createAssignTransition(le, foo->getExit(), builder->False());

    system->setMainAutomaton(foo);

    auto bmc = createBmcEngine(1);
    TraceBuilderImpl traceBuilder;

    auto result = bmc->check(*system, traceBuilder);
    ASSERT_TRUE(result->isFail());
    ASSERT_EQ(traceBuilder.capturedStates.size(), traceBuilder.capturedActions.size() + 1);
}

TEST_F(BmcTest, SimpleSuccess)
{
    Cfa* foo = system->createCfa("foo");
    Variable* x = foo->createLocal("x", IntType::Get(ctx));

    auto* l1 = foo->createLocation();
    auto* le = foo->createErrorLocation();
    foo->addErrorCode(le, builder->IntLit(1));

    foo->createAssignTransition(foo->getEntry(), l1, {{x, builder->IntLit(0)}});
    foo->createAssignTransition(l1, le, builder->NotEq(x->getRefExpr(), builder->IntLit(0)));
    foo->createAssignTransition(
        l1, foo->getExit(), builder->Eq(x->getRefExpr(), builder->IntLit(0)));

    system->setMainAutomaton(foo);

    auto bmc = createBmcEngine(1);
    TraceBuilderImpl traceBuilder;

    auto result = bmc->check(*system, traceBuilder);

    ASSERT_TRUE(result->isSuccess());
}

TEST_F(BmcTest, FailWithTailRecursiveProcedure)
{
    Cfa* foo = system->createCfa("foo");
    Variable* x = foo->createLocal("x", IntType::Get(ctx));

    Cfa* counter = this->createCounterCfa(3);

    auto* l1 = foo->createLocation();
    auto* le = foo->createErrorLocation();
    foo->addErrorCode(le, builder->IntLit(1));

    foo->createCallTransition(foo->getEntry(), l1, counter,
        { VariableAssignment{counter->getInput(0), builder->IntLit(0)} },
        { {x, counter->getOutput(0)->getRefExpr()} }
    );
    foo->createAssignTransition(l1, le, builder->Eq(x->getRefExpr(), builder->IntLit(3)));
    foo->createAssignTransition(
        l1, foo->getExit(), builder->NotEq(x->getRefExpr(), builder->IntLit(3)));

    system->setMainAutomaton(foo);
    TraceBuilderImpl traceBuilder;

    auto bmc1 = createBmcEngine(1);
    auto result = bmc1->check(*system, traceBuilder);
    ASSERT_EQ(result->getStatus(), VerificationResult::BoundReached);


    Location* l0InCounter = counter->getEntry();
    Location* exitInCounter = counter->getExit();
    Location* l2InCounter = counter->findLocationById(2);
    Location* l3InCounter = counter->findLocationById(3);
    std::vector<Location*> expectedTrace = {
        // Entering the main automaton
        foo->getEntry(),
        // First iteration
        l0InCounter,
        l2InCounter,
        l3InCounter,
        // Second iteration
        l0InCounter,
        l2InCounter,
        l3InCounter,
        // Third iteration
        l0InCounter,
        l2InCounter,
        // Going back through the "call stack"
        exitInCounter,
        exitInCounter,
        exitInCounter,
        // Back in 'foo'
        l1,
        le
    };

    auto bmc = createBmcEngine(3);
    result = bmc->check(*system, traceBuilder);

    ASSERT_TRUE(result->isFail());
    ASSERT_EQ(llvm::cast<FailResult>(*result).getErrorID(), 1);
    ASSERT_EQ(traceBuilder.capturedStates, expectedTrace);
}

TEST_F(BmcTest, EagerUnrolling)
{
    Cfa* foo = system->createCfa("foo");
    Variable* x = foo->createLocal("x", IntType::Get(ctx));

    Cfa* counter = this->createCounterCfa(10);

    auto* l1 = foo->createLocation();
    auto* le = foo->createErrorLocation();
    foo->addErrorCode(le, builder->IntLit(1));

    foo->createCallTransition(foo->getEntry(), l1, counter,
                              { VariableAssignment{counter->getInput(0), builder->IntLit(0)} },
                              { {x, counter->getOutput(0)->getRefExpr()} }
    );
    foo->createAssignTransition(l1, le, builder->Eq(x->getRefExpr(), builder->IntLit(10)));
    foo->createAssignTransition(
        l1, foo->getExit(), builder->NotEq(x->getRefExpr(), builder->IntLit(10)));

    system->setMainAutomaton(foo);

    BmcSettings settings{};
    settings.maxBound = 10;
    settings.eagerUnroll = 9;
    auto bmc = std::make_unique<BoundedModelChecker>(*solverFactory, settings);

    TraceBuilderImpl traceBuilder{};

    auto result = bmc->check(*system, traceBuilder);
    ASSERT_TRUE(result->isFail());
    ASSERT_EQ(llvm::cast<FailResult>(*result).getErrorID(), 1);
}

Cfa* BmcTest::createCounterCfa(int count)
{
    Cfa* cfa = system->createCfa("counter");

    auto* cnt = cfa->createInput("cnt", IntType::Get(ctx));
    auto* newCnt = cfa->createLocal("newCnt", IntType::Get(ctx));
    auto* resCnt = cfa->createLocal("res", IntType::Get(ctx));

    cfa->addOutput(resCnt);

    auto* l1 = cfa->createLocation();
    auto* l2 = cfa->createLocation();

    cfa->createAssignTransition(
        cfa->getEntry(), l1, {
        {newCnt, builder->Add(cnt->getRefExpr(), builder->IntLit(1))}
    });

    cfa->createAssignTransition(
        l1, cfa->getExit(),
        builder->Eq(newCnt->getRefExpr(), builder->IntLit(count)),
        { {resCnt, newCnt->getRefExpr()} });
    cfa->createAssignTransition(
        l1, l2, builder->Not(builder->Eq(newCnt->getRefExpr(), builder->IntLit(count))));
    cfa->createCallTransition(
        l2, cfa->getExit(), cfa, { {cnt, newCnt->getRefExpr()} }, { {resCnt, resCnt->getRefExpr() }}
    );

    return cfa;
}

} // namespace


