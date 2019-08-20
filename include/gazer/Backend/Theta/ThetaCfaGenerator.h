#ifndef GAZER_BACKEND_THETA_THETACFAGENERATOR_H
#define GAZER_BACKEND_THETA_THETACFAGENERATOR_H

#include "gazer/Core/Expr.h"

namespace gazer::theta
{

/// Transforms the input expression, removing every feature which
/// is unsupported by theta. This function performs the following transforms:
///     (1) Calculates the result of computations for literal expressions.
///     (2) Bit-logic expressions are replaced with Undef's.
///     (3) Bit-vector casts are either
///          (i) replaced by a no-op (ZExt, SExt),
///         (ii) replaced by a semantically safe linear arithmetic
///              expression (Extract, where applicable),
///        (iii) replaced by Undef's (every other case).
ExprPtr RewriteUnsupportedExprs(const ExprPtr& expr);

/// Prints the given expression in theta's expression format.
void PrintThetaExpr(const ExprPtr& expr, llvm::raw_ostream& os);

} // end namespace gazer::theta

#endif
