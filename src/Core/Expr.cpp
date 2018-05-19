#include "gazer/Core/Expr.h"

#include <llvm/Support/raw_ostream.h>

using namespace gazer;

std::string Expr::getKindName(ExprKind kind)
{
    const char* const Names[] = {
        "Literal",
        "VarRef",
        "Not",
        "Add",
        "Sub",
        "Mul",
        "Div",
        "And",
        "Or",
        "Xor",
        "Eq",
        "NotEq",
        "Lt",
        "LtEq",
        "Gt",
        "GtEq",
        "Select"
    };

    if (Expr::Literal <= kind && kind <= Expr::Select) {
        return Names[kind];
    }

    llvm_unreachable("Invalid expression kind.");
}

void NonNullaryExpr::print(std::ostream& os) const
{
    size_t i = 0;
    os << Expr::getKindName(getKind()) << "(";
    while (i < getNumOperands() - 1) {
        getOperand(i)->print(os);
        os << ",";
        ++i;
    }

    getOperand(i)->print(os);
    os << ")";
}

std::ostream& gazer::operator<<(std::ostream& os, const Expr& expr)
{
    expr.print(os);
    return os;
}
