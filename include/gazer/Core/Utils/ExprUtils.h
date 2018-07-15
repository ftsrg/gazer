#ifndef _GAZER_CORE_UTILS_EXPRUTILS_H
#define _GAZER_CORE_UTILS_EXPRUTILS_H

#include "gazer/Core/Expr.h"

namespace llvm {
    class raw_ostream;
}

namespace gazer
{

/**
 * Prints an expression in a formatted way.
 */
void FormatPrintExpr(const ExprPtr& expr, llvm::raw_ostream& os);


}

#endif
