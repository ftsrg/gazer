#ifndef GAZER_CORE_EXPR_EXPRUTILS_H
#define GAZER_CORE_EXPR_EXPRUTILS_H

#include "gazer/Core/Expr.h"

namespace gazer
{

unsigned ExprDepth(const ExprPtr& expr);

void FormatPrintExpr(const ExprPtr& expr, llvm::raw_ostream& os);

}

#endif