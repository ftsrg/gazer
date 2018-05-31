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
    /**
     * Handler for expression pointers.
     * This method should be overridden if you
     * wish to use the shared_ptr-based API.
     */
    virtual ReturnT visit(const ExprPtr& expr) {
        #define HANDLE_EXPRCASE(KIND)                                       \
            case Expr::KIND:                                                \
                return this->visit##KIND(std::static_pointer_cast<KIND##Expr>(expr));   \

        switch (expr->getKind()) {
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

        llvm_unreachable("Unknown expression kind");

        #undef HANDLE_EXPRCASE
    }

    virtual ~ExprVisitor() {}

protected:

    //--- Generic fallback methods ---//
    // In case you don't override specific expression classes,
    // these will be called as a fallback.
    
    /**
     * Basic fallback method, for unhandled instruction types.
     */
    virtual ReturnT visitExpr(const ExprPtr& expr) = 0;

    virtual ReturnT visitNonNullary(const std::shared_ptr<NonNullaryExpr>& expr) {
        return visitExpr(expr);
    }

    // Nullary
    virtual ReturnT visitLiteral(const std::shared_ptr<LiteralExpr>& expr) {
        return this->visitExpr(expr);
    }
    virtual ReturnT visitVarRef(const std::shared_ptr<VarRefExpr>& expr) {
        return this->visitExpr(expr);
    }

    // Unary
    virtual ReturnT visitNot(const std::shared_ptr<NotExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Binary
    virtual ReturnT visitAdd(const std::shared_ptr<AddExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitSub(const std::shared_ptr<SubExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitMul(const std::shared_ptr<MulExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitDiv(const std::shared_ptr<DivExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Logic
    virtual ReturnT visitAnd(const std::shared_ptr<AndExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitOr(const std::shared_ptr<OrExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitXor(const std::shared_ptr<XorExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Compare
    virtual ReturnT visitEq(const std::shared_ptr<EqExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitNotEq(const std::shared_ptr<NotEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitLt(const std::shared_ptr<LtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitLtEq(const std::shared_ptr<LtEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitGt(const std::shared_ptr<GtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitGtEq(const std::shared_ptr<GtEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Ternary
    virtual ReturnT visitSelect(const std::shared_ptr<SelectExpr>& expr) {
        return this->visitNonNullary(expr);
    }
};

} // end namespace gazer

#endif
