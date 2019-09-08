#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/ADT/APInt.h>

using namespace gazer;

std::unique_ptr<ExprBuilder> gazer::CreateExprBuilder(GazerContext& context)
{
    return std::unique_ptr<ExprBuilder>(new ExprBuilder(context));
}
