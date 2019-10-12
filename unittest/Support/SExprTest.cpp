#include "gazer/Support/SExpr.h"

#include <llvm/Support/raw_ostream.h>
#include <gtest/gtest.h>

using namespace gazer;

TEST(SExprTest, TestParseListOfAtoms)
{
    auto result = sexpr::parse("(A B C)");
    result->print(llvm::outs());

    result = sexpr::parse("(A  B\n\nC)");
    result->print(llvm::outs());

    result = sexpr::parse("(A (X Y Z))");
    result->print(llvm::outs());

    result = sexpr::parse("(A (X Y (Z)))");
    result->print(llvm::outs());
}