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
    ExprBuilder() {}

public:
    virtual ~ExprBuilder() {}

    //--- Literals ---//
    ExprPtr IntLit(uint64_t value, unsigned bits) {
        return IntLiteralExpr::get(*IntType::get(bits), value);
    }

    ExprPtr True()  { return BoolLiteralExpr::getTrue(); }
    ExprPtr False() { return BoolLiteralExpr::getFalse(); }
    ExprPtr Undef(const Type& type) { return UndefExpr::Get(type); }

    //--- Unary ---//
    virtual ExprPtr Not(const ExprPtr& op) = 0;
    virtual ExprPtr ZExt(const ExprPtr& op, const IntType& type) = 0;
    virtual ExprPtr SExt(const ExprPtr& op, const IntType& type) = 0;
    virtual ExprPtr Trunc(const ExprPtr& op, const IntType& type) = 0;

    //--- Binary ---//
    virtual ExprPtr Add(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Sub(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Mul(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Div(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Shl(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr LShr(const ExprPtr& left, const ExprPtr& right) = 0;    
    virtual ExprPtr AShr(const ExprPtr& left, const ExprPtr& right) = 0;    
    virtual ExprPtr BAnd(const ExprPtr& left, const ExprPtr& right) = 0;    
    virtual ExprPtr BOr(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr BXor(const ExprPtr& left, const ExprPtr& right) = 0;    

    //--- Logic ---//
    virtual ExprPtr And(const ExprVector& vector) = 0;
    virtual ExprPtr Or(const ExprVector& vector) = 0;

    ExprPtr And(const ExprPtr& left, const ExprPtr& right) {
        return this->And({left, right});
    }
    ExprPtr Or(const ExprPtr& left, const ExprPtr& right) {
        return this->Or({left, right});
    }

    template<class InputIterator>
    ExprPtr And(InputIterator begin, InputIterator end) {
        return this->And(ExprVector(begin, end));
    }
    template<class InputIterator>
    ExprPtr Or(InputIterator begin, InputIterator end) {
        return this->Or(ExprVector(begin, end));
    }

    virtual ExprPtr Xor(const ExprPtr& left, const ExprPtr& right) = 0;

    //--- Compare ---//
    virtual ExprPtr Eq(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr NotEq(const ExprPtr& left, const ExprPtr& right) = 0;

    virtual ExprPtr SLt(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr SLtEq(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr SGt(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr SGtEq(const ExprPtr& left, const ExprPtr& right) = 0;

    virtual ExprPtr ULt(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr ULtEq(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr UGt(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr UGtEq(const ExprPtr& left, const ExprPtr& right) = 0;

    //--- Ternary ---//
    virtual ExprPtr Select(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze) = 0;
};

std::unique_ptr<ExprBuilder> CreateExprBuilder();
std::unique_ptr<ExprBuilder> CreateFoldingExprBuilder();

}

#endif
