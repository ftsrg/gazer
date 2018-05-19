#ifndef _GAZER_CORE_LITERALEXPR_H
#define _GAZER_CORE_LITERALEXPR_H

#include "gazer/Core/Expr.h"

namespace gazer
{

class IntLiteralExpr : public LiteralExpr
{
private:
    IntLiteralExpr(long value)
        : LiteralExpr(*IntType::get()), mValue(value)
    {}

public:
    static std::shared_ptr<IntLiteralExpr> get(long value);

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isIntType();
    }

private:
    long mValue;
};

}

#endif
