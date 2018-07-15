#include "gazer/Core/Expr.h"
#include "gazer/Core/ExprTypes.h"

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_os_ostream.h>

using namespace gazer;

std::string Expr::getKindName(ExprKind kind)
{
    const char* const Names[] = {
        "Undef",
        "Literal",
        "VarRef",
        "Not",
        "ZExt",
        "SExt",
        "Trunc",
        "Add",
        "Sub",
        "Mul",
        "Div",
        "Shl",
        "LShr",
        "AShr",
        "BAnd",
        "BOr",
        "BXor",
        "And",
        "Or",
        "Xor",
        "Eq",
        "NotEq",
        "SLt",
        "SLtEq",
        "SGt",
        "SGtEq",
        "ULt",
        "ULtEq",
        "UGt",
        "UGtEq",
        "Select"
    };

    static_assert(
        (sizeof(Names) / sizeof(Names[0])) == LastExprKind + 1,
        "Missing ExprKind in Expr::print()"
    );

    if (Expr::FirstExprKind <= kind && kind <= Expr::LastExprKind) {
        return Names[kind];
    }

    llvm_unreachable("Invalid expression kind.");
}

void NonNullaryExpr::print(llvm::raw_ostream& os) const
{
    size_t i = 0;
    os << getType().getName() << " " << Expr::getKindName(getKind()) << "(";
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
    llvm::raw_os_ostream ros(os);
    expr.print(ros);
    return os;
}

std::shared_ptr<SelectExpr> SelectExpr::Create(ExprPtr condition, ExprPtr then, ExprPtr elze)
{
    assert(then->getType() == elze->getType() && "Select expression operand types must match.");
    assert(condition->getType().isBoolType() && "Select expression condition type must be boolean.");
    return std::shared_ptr<SelectExpr>(new SelectExpr(then->getType(), condition, then, elze));
}
