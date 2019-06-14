#ifndef GAZER_CORE_EXPR_EXPRREWRITE_H
#define GAZER_CORE_EXPR_EXPRREWRITE_H

#include "gazer/Core/ExprVisitor.h"
#include "gazer/Core/Expr/ExprBuilder.h"

namespace gazer
{

class ExprRewrite : public ExprVisitor<ExprPtr>
{
public:
    ExprRewrite(ExprBuilder& builder);
    ExprPtr& operator[](Variable* variable);

protected:
    ExprPtr visitExpr(const ExprPtr& expr) override;
    ExprPtr visitVarRef(const ExprRef<VarRefExpr>& expr) override;
    ExprPtr visitNonNullary(const ExprRef<NonNullaryExpr>& expr) override;

private:
    llvm::DenseMap<Variable*, ExprPtr> mRewriteMap;
    ExprBuilder& mExprBuilder;
};

}

#endif
