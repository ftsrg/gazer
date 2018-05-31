#ifndef _GAZER_CORE_LITERALEXPR_H
#define _GAZER_CORE_LITERALEXPR_H

#include "gazer/Core/Expr.h"

#include <llvm/ADT/APInt.h>

namespace gazer
{

class BoolLiteralExpr final : public LiteralExpr
{
private:
    BoolLiteralExpr(bool value)
        : LiteralExpr(*BoolType::get()), mValue(value)
    {}

public:
    static std::shared_ptr<BoolLiteralExpr> getTrue();
    static std::shared_ptr<BoolLiteralExpr> getFalse();

    virtual void print(std::ostream& os) const override;
    bool getValue() const { return mValue; }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isBoolType();
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Literal && expr.getType().isBoolType();
    }
private:
    bool mValue;
};

class IntLiteralExpr final : public LiteralExpr
{
private:
    IntLiteralExpr(IntType& type, uint64_t value)
        : LiteralExpr(type), mValue(value)
    {}
public:
    virtual void print(std::ostream& os) const override;

public:
    static std::shared_ptr<IntLiteralExpr> get(IntType& type, uint64_t value);

    uint64_t getValue() const { return mValue; }
    const IntType& getType() const {
        return static_cast<const IntType&>(mType);
    }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isIntType();
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Literal && expr.getType().isIntType();
    }

private:
    uint64_t mValue;
};

}

#endif
