#include "gazer/Support/SExpr.h"

#include <llvm/Support/raw_ostream.h>
#include <gtest/gtest.h>

using namespace gazer;

TEST(SExprTest, TestParse)
{
    std::unique_ptr<sexpr::Value> expected;

    expected.reset(sexpr::list({
        sexpr::atom("A"),
        sexpr::atom("B"),
        sexpr::atom("C")
    }));
    EXPECT_EQ(*sexpr::parse("(A B\n \nC)"), *expected);

    expected.reset(sexpr::list({
        sexpr::atom("A"),
        sexpr::list({
            sexpr::atom("X"),
            sexpr::atom("Y"),
            sexpr::atom("Z")
        })
    }));
    EXPECT_EQ(*sexpr::parse("(A (X Y Z))"), *expected);

    expected.reset(sexpr::list({
        sexpr::atom("A"),
        sexpr::list({
            sexpr::atom("X"),
            sexpr::atom("Y"),
            sexpr::list({ sexpr::atom("Z") })
        })
    }));
    EXPECT_EQ(*sexpr::parse("(A (X Y (Z)))"), *expected);
}