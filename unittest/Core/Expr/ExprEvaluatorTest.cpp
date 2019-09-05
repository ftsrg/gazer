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
    ExprEvaluator eval{vb.build()};
    auto actual = eval.walk(expr);

    EXPECT_TRUE(llvm::isa<BoolLiteralExpr>(actual));
    EXPECT_EQ(actual, expected);
}

void checkBv(Valuation::Builder& vb, const ExprPtr& expr, llvm::APInt expected)
{
    ExprEvaluator eval{vb.build()};
    auto lit = eval.walk(expr);

    ASSERT_TRUE(llvm::isa<BvLiteralExpr>(lit));
    EXPECT_EQ(llvm::cast<BvLiteralExpr>(lit)->getValue(), expected);
}

void checkBv(Valuation::Builder& vb, const ExprPtr& expr, uint64_t expected)
{
    checkBv(vb, expr, llvm::APInt{32, expected});
}

TEST_F(ExprEvalTest, TestLiterals)
{
    auto vb = Valuation::CreateBuilder();
    ExprEvaluator eval{vb.build()};

    EXPECT_EQ(eval.walk(builder->True()), builder->True());
    EXPECT_EQ(eval.walk(builder->False()), builder->False());

    EXPECT_EQ(eval.walk(builder->BvLit(0, 32)), builder->BvLit(0, 32));
    EXPECT_EQ(eval.walk(builder->BvLit(1, 32)), builder->BvLit(1, 32));

    EXPECT_EQ(
        eval.walk(builder->FloatLit(llvm::APFloat{0.00})),
        builder->FloatLit(llvm::APFloat{0.00})
    );
    EXPECT_EQ(
        eval.walk(builder->FloatLit(llvm::APFloat{1.0})),
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

    ExprEvaluator eval{vb.build()};

    // Add
    checkBv(vb, builder->Add(x, y), 7);
    checkBv(vb, builder->Add(y, z), 18);

    // Add with overflow
    checkBv(
        vb,
        builder->Add(builder->BvLit(1, 32), builder->BvLit(llvm::APInt::getMaxValue(32))),
        0
    );

    // Sub
    checkBv(vb, builder->Sub(y, x), 7);
    checkBv(vb, builder->Sub(z, y), 4);

    // Sub with underflow
    checkBv(
        vb,
        builder->Sub(x, builder->BvLit(1, 32)),
        llvm::APInt::getMaxValue(32)
    );

    // Mul
    checkBv(vb, builder->Mul(x, y), 0);
    checkBv(vb, builder->Mul(y, z), 77);
}

TEST_F(ExprEvalTest, TestEq)
{
    auto vb = Valuation::CreateBuilder();
    ExprEvaluator eval{vb.build()};

    auto zero = builder->BvLit(0, 32);
    auto one  = builder->BvLit(1, 32);
    auto two  = builder->BvLit(2, 32);

    EXPECT_EQ(eval.walk(builder->Eq(one, one)), builder->True());
    EXPECT_EQ(eval.walk(builder->Eq(one, two)), builder->False());

    EXPECT_EQ(eval.walk(builder->Eq(builder->True(), builder->True())), builder->True());
    EXPECT_EQ(eval.walk(builder->Eq(builder->False(), builder->True())), builder->False());
}

TEST_F(ExprEvalTest, TestSignedCompareBv)
{
    auto vb = Valuation::CreateBuilder();
    ExprEvaluator eval{vb.build()};

    auto zero = builder->BvLit(0, 32);
    auto one  = builder->BvLit(1, 32);
    auto two  = builder->BvLit(2, 32);
    auto minusOne = builder->BvLit(llvm::APInt{32, static_cast<uint64_t>(-1), true});
    auto minusTwo = builder->BvLit(llvm::APInt{32, static_cast<uint64_t>(-2), true});


    EXPECT_EQ(eval.walk(builder->BvSLt(one, two)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSLt(minusOne, zero)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSLt(minusTwo, minusOne)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSLt(minusOne, minusOne)), builder->False());
    EXPECT_EQ(eval.walk(builder->BvSLt(two, minusTwo)), builder->False());

    EXPECT_EQ(eval.walk(builder->BvSLtEq(one, two)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSLtEq(minusOne, zero)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSLtEq(minusTwo, minusOne)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSLtEq(minusOne, minusOne)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSLtEq(two, minusTwo)), builder->False());

    EXPECT_EQ(eval.walk(builder->BvSGt(one, zero)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSGt(minusOne, zero)), builder->False());
    EXPECT_EQ(eval.walk(builder->BvSGt(two, one)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSGt(one, two)), builder->False());
    EXPECT_EQ(eval.walk(builder->BvSGt(zero, zero)), builder->False());
    EXPECT_EQ(eval.walk(builder->BvSGt(minusTwo, minusOne)), builder->False());
    EXPECT_EQ(eval.walk(builder->BvSGt(minusOne, minusOne)), builder->False());
    EXPECT_EQ(eval.walk(builder->BvSGt(two, minusTwo)), builder->True());

    EXPECT_EQ(eval.walk(builder->BvSGtEq(one, zero)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSGtEq(minusOne, zero)), builder->False());
    EXPECT_EQ(eval.walk(builder->BvSGtEq(two, one)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSGtEq(one, two)), builder->False());
    EXPECT_EQ(eval.walk(builder->BvSGtEq(zero, zero)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSGtEq(minusTwo, minusOne)), builder->False());
    EXPECT_EQ(eval.walk(builder->BvSGtEq(minusOne, minusOne)), builder->True());
    EXPECT_EQ(eval.walk(builder->BvSGtEq(two, minusTwo)), builder->True());
}