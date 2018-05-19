#ifndef _GAZER_CORE_LITERALEXPR_H
#define _GAZER_CORE_LITERALEXPR_H

#include "gazer/Core/Expr.h"

namespace gazer
{

class IntLiteralExpr : public LiteralExpr
{
private:
    IntLiteralExpr(int value)
        : LiteralExpr(*IntType::get()), mValue(value)
    {}
public:
    virtual void print(std::ostream& os) const override;

public:
    static std::shared_ptr<IntLiteralExpr> get(int value);

    int getValue() const { return mValue; }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isIntType();
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Literal && expr.getType().isIntType();
    }

private:
    int mValue;
};

}

#endif
