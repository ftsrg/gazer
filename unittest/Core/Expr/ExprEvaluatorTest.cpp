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
#include "gazer/Core/Expr/ExprEvaluator.h"
#include "gazer/Core/Expr/ExprBuilder.h"

#include <llvm/Support/raw_ostream.h>

#include <gtest/gtest.h>

using namespace gazer;

class ExprEvalTest : public ::testing::Test
{
protected:
    GazerContext context;
    std::unique_ptr<ExprBuilder> builder;

    ExprRef<VarRefExpr> a, b, c, d;
    ExprRef<VarRefExpr> x, y, z;

public:
    ExprEvalTest()
        : builder(CreateExprBuilder(context))
    {
        a = context.createVariable("a", BoolType::Get(context))->getRefExpr();
        b = context.createVariable("b", BoolType::Get(context))->getRefExpr();
        c = context.createVariable("c", BoolType::Get(context))->getRefExpr();
        d = context.createVariable("d", BoolType::Get(context))->getRefExpr();

        x = context.createVariable("x", BvType::Get(context, 32))->getRefExpr();
        y = context.createVariable("y", BvType::Get(context, 32))->getRefExpr();
        z = context.createVariable("z", BvType::Get(context, 32))->getRefExpr();
    }
};

void checkBoolEval(
    Valuation::Builder& vb,
    const ExprPtr& expr,
    const ExprRef<LiteralExpr>& expected)
{
    auto valuation = vb.build();
    ValuationExprEvaluator eval(valuation);
    auto actual = eval.evaluate(expr);

    EXPECT_TRUE(llvm::isa<BoolLiteralExpr>(actual));
    EXPECT_EQ(actual, expected);
}

void checkBv(Valuation::Builder& vb, const ExprPtr& expr, llvm::APInt expected)
{
    auto valuation = vb.build();
    ValuationExprEvaluator eval(valuation);
    auto lit = eval.evaluate(expr);

    ASSERT_TRUE(llvm::isa<BvLiteralExpr>(lit));
    EXPECT_EQ(llvm::cast<BvLiteralExpr>(lit)->getValue(), expected);
}

void checkBv(Valuation::Builder& vb, const ExprPtr& expr, uint64_t expected)
{
    checkBv(vb, expr, llvm::APInt{32, expected});
}

TEST_F(ExprEvalTest, TestLiterals)
{
    Valuation valuation = Valuation::CreateBuilder().build();
    ValuationExprEvaluator eval{valuation};

    EXPECT_EQ(eval.evaluate(builder->True()), builder->True());
    EXPECT_EQ(eval.evaluate(builder->False()), builder->False());

    EXPECT_EQ(eval.evaluate(builder->BvLit(0, 32)), builder->BvLit(0, 32));
    EXPECT_EQ(eval.evaluate(builder->BvLit(1, 32)), builder->BvLit(1, 32));

    EXPECT_EQ(
        eval.evaluate(builder->FloatLit(llvm::APFloat{0.00})),
        builder->FloatLit(llvm::APFloat{0.00})
    );
    EXPECT_EQ(
        eval.evaluate(builder->FloatLit(llvm::APFloat{1.0})),
        builder->FloatLit(llvm::APFloat{1.0})
    );
}

TEST_F(ExprEvalTest, TestNot)
{
    auto vb = Valuation::CreateBuilder();
    vb.put(&a->getVariable(), builder->BoolLit(false));

    auto expr = builder->Not(a);
    
    checkBoolEval(vb, expr, builder->True());
}

TEST_F(ExprEvalTest, TestAnd)
{
    auto vb = Valuation::CreateBuilder();
    vb.put(&a->getVariable(), builder->BoolLit(false));
    vb.put(&b->getVariable(), builder->BoolLit(true));
    vb.put(&c->getVariable(), builder->BoolLit(true));

    checkBoolEval(
        vb, builder->And(a, b), builder->False()
    );
    checkBoolEval(
        vb, builder->And(b, b), builder->True()
    );
}

TEST_F(ExprEvalTest, TestXor)
{
    auto vb = Valuation::CreateBuilder();
    vb.put(&a->getVariable(), builder->BoolLit(false));
    vb.put(&b->getVariable(), builder->BoolLit(true));
    vb.put(&c->getVariable(), builder->BoolLit(false));

    checkBoolEval(
        vb, builder->Xor(a, b), builder->True()
    );
    checkBoolEval(
        vb, builder->Xor(a, c), builder->False()
    );
}

TEST_F(ExprEvalTest, TestImply)
{
    auto vb = Valuation::CreateBuilder();
    vb.put(&a->getVariable(), builder->BoolLit(false));
    vb.put(&b->getVariable(), builder->BoolLit(true));
    vb.put(&c->getVariable(), builder->BoolLit(false));
    vb.put(&d->getVariable(), builder->BoolLit(true));

    // False => True   === True
    // True  => False  === False
    // False => False  === True
    // True  => True   === True
    checkBoolEval(
        vb, builder->Imply(a, b), builder->True()
    );
    checkBoolEval(
        vb, builder->Imply(b, c), builder->False()
    );
    checkBoolEval(
        vb, builder->Imply(a, c), builder->True()
    );
    checkBoolEval(
        vb, builder->Imply(b, d), builder->True()
    );
}

TEST_F(ExprEvalTest, TestBvArithmetic)
{
    auto vb = Valuation::CreateBuilder();
    vb.put(&x->getVariable(), builder->BvLit(0, 32));
    vb.put(&y->getVariable(), builder->BvLit(7, 32));
    vb.put(&z->getVariable(), builder->BvLit(11, 32));

    ValuationExprEvaluator eval{vb.build()};

    // Add
    checkBv(vb, builder->Add(x, y), 7);
    checkBv(vb, builder->Add(y, z), 18);

    // Add with overflow
    checkBv(vb, builder->Add(builder->BvLit(1, 32), builder->BvLit(llvm::APInt::getMaxValue(32))), 0);

    // Sub
    checkBv(vb, builder->Sub(y, x), 7);
    checkBv(vb, builder->Sub(z, y), 4);

    // Sub with underflow
    checkBv(vb, builder->Sub(x, builder->BvLit(1, 32)), llvm::APInt::getMaxValue(32));

    // Mul
    checkBv(vb, builder->Mul(x, y), 0);
    checkBv(vb, builder->Mul(y, z), 77);
}

TEST_F(ExprEvalTest, TestEq)
{
    auto vb = Valuation::CreateBuilder();
    ValuationExprEvaluator eval{vb.build()};

    auto zero = builder->BvLit(0, 32);
    auto one  = builder->BvLit(1, 32);
    auto two  = builder->BvLit(2, 32);

    EXPECT_EQ(eval.evaluate(builder->Eq(one, one)), builder->True());
    EXPECT_EQ(eval.evaluate(builder->Eq(one, two)), builder->False());

    EXPECT_EQ(eval.evaluate(builder->Eq(builder->True(), builder->True())), builder->True());
    EXPECT_EQ(eval.evaluate(builder->Eq(builder->False(), builder->True())), builder->False());
}

// Helper macros for comparisons
#define TRUE_COMPARE(KIND, OP1, OP2)                                       \
    EXPECT_EQ(eval.evaluate(builder->KIND(OP1, OP2)), builder->True())
#define FALSE_COMPARE(KIND, OP1, OP2)                                       \
    EXPECT_EQ(eval.evaluate(builder->KIND(OP1, OP2)), builder->False())

TEST_F(ExprEvalTest, TestSignedCompareBv)
{
    auto vb = Valuation::CreateBuilder();
    ValuationExprEvaluator eval{vb.build()};

    auto zero = builder->BvLit(0, 32);
    auto one  = builder->BvLit(1, 32);
    auto two  = builder->BvLit(2, 32);
    auto minusOne = builder->BvLit(llvm::APInt{32, static_cast<uint64_t>(-1), true});
    auto minusTwo = builder->BvLit(llvm::APInt{32, static_cast<uint64_t>(-2), true});

    TRUE_COMPARE(BvSLt, one, two);
    TRUE_COMPARE(BvSLt, minusOne, zero);
    TRUE_COMPARE(BvSLt, minusTwo, minusOne);
    FALSE_COMPARE(BvSLt, minusOne, minusOne);
    FALSE_COMPARE(BvSLt, two, minusTwo);

    TRUE_COMPARE(BvSLtEq, one, two);
    TRUE_COMPARE(BvSLtEq, minusOne, zero);
    TRUE_COMPARE(BvSLtEq, minusTwo, minusOne);
    TRUE_COMPARE(BvSLtEq, minusOne, minusOne);
    FALSE_COMPARE(BvSLtEq, two, minusTwo);

    TRUE_COMPARE(BvSGt, one, zero);
    FALSE_COMPARE(BvSGt, minusOne, zero);
    TRUE_COMPARE(BvSGt, two, one);
    FALSE_COMPARE(BvSGt, one, two);
    FALSE_COMPARE(BvSGt, zero, zero);
    FALSE_COMPARE(BvSGt, minusTwo, minusOne);
    FALSE_COMPARE(BvSGt, minusOne, minusOne);
    TRUE_COMPARE(BvSGt, two, minusTwo);

    TRUE_COMPARE(BvSGtEq, one, zero);
    FALSE_COMPARE(BvSGtEq, minusOne, zero);
    TRUE_COMPARE(BvSGtEq, two, one);
    FALSE_COMPARE(BvSGtEq, one, two);
    TRUE_COMPARE(BvSGtEq, zero, zero);
    FALSE_COMPARE(BvSGtEq, minusTwo, minusOne);
    TRUE_COMPARE(BvSGtEq, minusOne, minusOne);
    TRUE_COMPARE(BvSGtEq, two, minusTwo);
}

#undef TRUE_COMPARE
#undef FALSE_COMPARE