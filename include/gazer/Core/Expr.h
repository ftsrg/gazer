#ifndef _GAZER_CORE_EXPR_H
#define _GAZER_CORE_EXPR_H

#include "gazer/Core/Type.h"

#include <memory>
#include <string>
#include <iosfwd>

namespace gazer
{

/**
 * Base class for all gazer expressions.
 */
class Expr
{
    friend class ExprRef;
public:
    enum ExprKind
    {
        // Nullary
        Literal = 0,
        VarRef,

        // Unary logic
        Not,

        // Binary arithmetic
        Add,
        Sub,
        Mul,
        Div,

        // Binary logic
        And,
        Or,
        Xor,

        // Compare
        Eq,
        NotEq,
        Lt,
        LtEq,
        Gt,
        GtEq,

        // Ternary
        Select
    };

    static constexpr int FirstUnary = Not;
    static constexpr int LastUnary = Not;
    static constexpr int FirstBinaryArithmetic = Add;
    static constexpr int LastBinaryArithmetic = Div;
    static constexpr int FirstLogic = And;
    static constexpr int LastLogic = Xor;
    static constexpr int FirstCompare = Eq;
    static constexpr int LastCompare = GtEq;

    static constexpr int FirstExprKind = Literal;
    static constexpr int LastExprKind = Select;

protected:
    Expr(ExprKind kind, const Type& type)
        : mKind(kind), mType(type)
    {}

public:
    ExprKind getKind() const { return mKind; }
    const Type& getType() const { return mType; }

    void print(std::ostream& os) const;

    bool isNullary() const { return mKind <= FirstUnary; }
    bool isUnary() const { return FirstUnary <= mKind && mKind <= LastUnary; }

    bool isArithmetic() const {
        return FirstBinaryArithmetic <= mKind && mKind <= LastBinaryArithmetic;
    }
    bool isLogic() const {
        return FirstLogic <= mKind && mKind <= LastLogic;
    }
    bool isCompare() const {
        return FirstCompare <= mKind && mKind <= LastCompare;
    }


public:
    static std::string getKindName(ExprKind kind);

private:
    ExprKind mKind;
    const Type& mType;
};

using ExprPtr = std::shared_ptr<Expr>;

std::ostream& operator<<(std::ostream& os, const Expr& expr);

class LiteralExpr : public Expr
{
protected:
    LiteralExpr(const Type& type)
        : Expr(Literal, type)
    {}
public:
    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal;
    }
};

}

#endif
