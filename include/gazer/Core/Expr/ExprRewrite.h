#ifndef GAZER_CORE_EXPR_EXPRREWRITE_H
#define GAZER_CORE_EXPR_EXPRREWRITE_H

#include "gazer/Core/Expr/ExprWalker.h"
#include "gazer/Core/Expr/ExprBuilder.h"

namespace gazer
{

class ExprRewrite : public ExprWalker<ExprRewrite, ExprPtr>
{
public:
    ExprRewrite(ExprBuilder& builder);
    ExprPtr& operator[](Variable* variable);

public:
    ExprPtr visitExpr(const ExprPtr& expr);
    ExprPtr visitVarRef(const ExprRef<VarRefExpr>& expr);
    ExprPtr visitNonNullary(const ExprRef<NonNullaryExpr>& expr);

private:
    llvm::DenseMap<Variable*, ExprPtr> mRewriteMap;
    ExprBuilder& mExprBuilder;
};

}

#endif
