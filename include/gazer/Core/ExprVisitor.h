#ifndef _GAZER_CORE_EXPRVISITOR_H
#define _GAZER_CORE_EXPRVISITOR_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Variable.h"

namespace gazer
{

template<class ReturnT = void>
class ExprVisitor
{
public:
    ReturnT visit(ExprPtr expr) { return visit(*expr); }

    virtual ReturnT visit(Expr& expr) {
        #define HANDLE_EXPRCASE(KIND)           \
            case Expr::KIND:                    \
                return this->visit##KIND(static_cast<KIND##Expr&>(expr));     \    

        switch (expr.getKind()) {
            HANDLE_EXPRCASE(Literal)
            HANDLE_EXPRCASE(VarRef)
            HANDLE_EXPRCASE(Not)
            HANDLE_EXPRCASE(Add)
            HANDLE_EXPRCASE(Sub)
            HANDLE_EXPRCASE(Mul)
            HANDLE_EXPRCASE(Div)
            HANDLE_EXPRCASE(And)
            HANDLE_EXPRCASE(Or)
            HANDLE_EXPRCASE(Xor)
            HANDLE_EXPRCASE(Eq)
            HANDLE_EXPRCASE(NotEq)
            HANDLE_EXPRCASE(Lt)
            HANDLE_EXPRCASE(LtEq)
            HANDLE_EXPRCASE(Gt)
            HANDLE_EXPRCASE(GtEq)
            HANDLE_EXPRCASE(Select)
        }

        #undef HANDLE_EXPRCASE
    }

    // Nullary
    virtual ReturnT visitLiteral(LiteralExpr& expr) = 0;
    virtual ReturnT visitVarRef(VarRefExpr& expr) = 0;

    // Unary
    virtual ReturnT visitNot(NotExpr& expr) = 0;

    // Binary
    virtual ReturnT visitAdd(AddExpr& expr) = 0;
    virtual ReturnT visitSub(SubExpr& expr) = 0;
    virtual ReturnT visitMul(MulExpr& expr) = 0;
    virtual ReturnT visitDiv(DivExpr& expr) = 0;

    // Logic
    virtual ReturnT visitAnd(AndExpr& expr) = 0;
    virtual ReturnT visitOr(OrExpr& expr) = 0;
    virtual ReturnT visitXor(XorExpr& expr) = 0;

    // Compare
    virtual ReturnT visitEq(EqExpr& expr) = 0;
    virtual ReturnT visitNotEq(NotEqExpr& expr) = 0;
    virtual ReturnT visitLt(LtExpr& expr) = 0;
    virtual ReturnT visitLtEq(LtEqExpr& expr) = 0;
    virtual ReturnT visitGt(GtExpr& expr) = 0;
    virtual ReturnT visitGtEq(GtEqExpr& expr) = 0;

    // Ternary
    virtual ReturnT visitSelect(SelectExpr& expr) = 0;
};

}

#endif
