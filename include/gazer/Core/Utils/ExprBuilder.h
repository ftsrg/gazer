#ifndef _GAZER_CORE_UTILS_EXPRBUILDER_H
#define _GAZER_CORE_UTILS_EXPRBUILDER_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

namespace llvm
{
    class APInt;
}

namespace gazer
{

class ExprBuilder
{
protected:
    ExprBuilder();

public:
    virtual ~ExprBuilder() {}

    //--- Literals ---//
    ExprPtr IntLit(uint64_t value, unsigned bits) {
        return IntLiteralExpr::get(*IntType::get(bits), value);
    }

    ExprPtr True()  { return BoolLiteralExpr::getTrue(); }
    ExprPtr False() { return BoolLiteralExpr::getFalse(); }

    //--- Unary ---//
    virtual ExprPtr Not(const ExprPtr& op) = 0;

    //--- Binary ---//
    virtual ExprPtr Add(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Sub(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Mul(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Div(const ExprPtr& left, const ExprPtr& right) = 0;

    virtual ExprPtr And(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Or(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Xor(const ExprPtr& left, const ExprPtr& right) = 0;

    virtual ExprPtr Eq(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr NotEq(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Lt(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr LtEq(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Gt(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr GtEq(const ExprPtr& left, const ExprPtr& right) = 0;

    //--- Ternary ---//
    virtual ExprPtr Select(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze) = 0;
};

std::unique_ptr<ExprBuilder> CreateExprBuilder();
std::unique_ptr<ExprBuilder> CreateFoldingExprBuilder();

}

#endif
