#include "gazer/Core/Expr.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <iostream>

using namespace gazer;

static void getOperatorName(Expr* expr) {
    const char* const const const Names[] = {
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
}

void Expr::print(std::ostream& os) const
{
    
}
