#include "gazer/Core/Expr/ExprRewrite.h"

using namespace gazer;

ExprPtr ExprRewrite::visitExpr(const ExprPtr& expr)
{
    return expr;
}

ExprPtr ExprRewrite::visitVarRef(const ExprRef<VarRefExpr>& expr)
{
    auto result = mRewriteMap[&expr->getVariable()];
    if (result != nullptr) {
        return result;
    }

    return expr;
}

ExprPtr ExprRewrite::visitNonNullary(const ExprRef<NonNullaryExpr>& expr)
{
    ExprVector ops;
    for (ExprPtr operand : expr->operands()) {
        ops.push_back(this->visit(operand));
    }

    return expr->clone(ops);
}

ExprPtr& ExprRewrite::operator[](Variable* variable)
{
    return mRewriteMap[variable];
}
