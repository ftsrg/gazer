#include "gazer/Core/Expr/Matcher.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/SymbolTable.h"
#include "gazer/Core/Variable.h"

#include <gtest/gtest.h>

using namespace gazer;
using namespace gazer::PatternMatch;

class MatcherTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        symbols = std::make_unique<SymbolTable>();
        builder = CreateExprBuilder();

        A = symbols->create("A", BvType::get(32)).getRefExpr();
        B = symbols->create("B", BoolType::get()).getRefExpr();
        C = symbols->create("C", BvType::get(32)).getRefExpr();
        D = symbols->create("D", BvType::get(32)).getRefExpr();

        // A + (undef * 5)
        E1 = builder->Add(
            A, builder->Mul(
                builder->Undef(BvType::get(32)),
                builder->BvLit(5, 32)
            )
        );

        // B & (A = 5)
        E2 = builder->And(
            B, builder->Eq(A, builder->BvLit(5, 32))
        );

        // (A = D) & (B | True) & B
        E3 = builder->And({
            builder->Eq(A, D),
            builder->Or(B, builder->True()),
            B
        });
    }

    ExprRef<VarRefExpr> A, B, C, D;
    ExprRef<> E1, E2, E3;

    std::unique_ptr<SymbolTable> symbols;
    std::unique_ptr<ExprBuilder> builder;
};

TEST_F(MatcherTest, MatchAndIgnore)
{
    ASSERT_TRUE(match(E1, m_Expr()));

    auto pattern = m_Add(m_Expr(), m_Mul(m_Undef(), m_Literal()));

    ASSERT_TRUE(match(E1, pattern));
    ASSERT_FALSE(match(E2, pattern));

    // Addition is commutative, so the reversed pattern should work as well
    auto revPattern = m_Add(m_Mul(m_Undef(), m_Literal()), m_Expr());

    ASSERT_TRUE(match(E1, revPattern));
    ASSERT_FALSE(match(E2, revPattern));
}

TEST_F(MatcherTest, MatchLiterals)
{
    llvm::APInt val;

    auto pattern = m_And(m_Expr(), m_Eq(m_Expr(), m_Bv(&val)));

    bool matched = match(E2, pattern);
    ASSERT_TRUE(matched);
    ASSERT_EQ(val, 5);

    ASSERT_FALSE(match(E1, pattern));
}

TEST_F(MatcherTest, MatchMultiaryOrdered)
{
    auto pattern = m_Ordered_And(m_Eq(m_Expr(), m_Expr()), m_Expr(), m_Expr());
    
    bool matched = match(E3, pattern);
    ASSERT_TRUE(matched);

    auto invalid = m_Ordered_And(m_Sub(m_Expr(), m_Expr()), m_Expr(), m_Expr());
    matched = match(E3, invalid);
    ASSERT_FALSE(matched);
}

TEST_F(MatcherTest, MatchMultiaryUnordered)
{
    auto pattern = m_And(m_Eq(m_Expr(), m_Expr()), m_Expr(), m_Expr());
    
    bool matched = match(E3, pattern);
    ASSERT_TRUE(matched);

    auto shuffled = m_And(m_Or(m_Expr(), m_Expr()), m_Eq(m_Expr(), m_Expr()), m_VarRef());
    matched = match(E3, shuffled);
    ASSERT_TRUE(matched);
}

TEST_F(MatcherTest, MatchVector)
{
    ExprVector exprs = { E1, E2, E3 };
    ExprVector unmatched;

    bool matched = unord_match(
        exprs, unmatched,
        m_And(m_Eq(m_Expr(), m_Expr()), m_Expr(), m_Expr()),
        m_And(m_Expr(), m_Eq(m_Expr(), m_Expr())));

    ASSERT_TRUE(matched);
    ASSERT_EQ(unmatched.size(), 1);
}
