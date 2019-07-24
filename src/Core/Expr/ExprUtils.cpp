#include "gazer/Core/Expr/ExprUtils.h"

#include <numeric>

using namespace gazer;

unsigned gazer::ExprDepth(const ExprPtr& expr)
{
    if (expr->isNullary()) {
        return 1;
    }

    if (auto nn = llvm::dyn_cast<NonNullaryExpr>(expr.get())) {
        unsigned max = 0;
        for (auto& op : nn->operands()) {
            unsigned d = ExprDepth(op);
            if (d > max) {
                max = d;
            }
        }

        return 1 + max;
    }

    llvm_unreachable("An expression cannot be nullary and non-nullary at the same time!");
}
