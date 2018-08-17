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
     */
    virtual ReturnT visit(const ExprPtr& expr) {
        #define HANDLE_EXPRCASE(KIND)                                       \
            case Expr::KIND:                                                \
                return this->visit##KIND(std::static_pointer_cast<KIND##Expr>(expr));   \

        switch (expr->getKind()) {
            HANDLE_EXPRCASE(Undef)
            HANDLE_EXPRCASE(Literal)
            HANDLE_EXPRCASE(VarRef)
            HANDLE_EXPRCASE(Not)
            HANDLE_EXPRCASE(ZExt)
            HANDLE_EXPRCASE(SExt)
            HANDLE_EXPRCASE(Extract)
            HANDLE_EXPRCASE(Add)
            HANDLE_EXPRCASE(Sub)
            HANDLE_EXPRCASE(Mul)
            HANDLE_EXPRCASE(Div)
            HANDLE_EXPRCASE(Shl)
            HANDLE_EXPRCASE(LShr)
            HANDLE_EXPRCASE(AShr)
            HANDLE_EXPRCASE(BAnd)
            HANDLE_EXPRCASE(BOr)
            HANDLE_EXPRCASE(BXor)
            HANDLE_EXPRCASE(And)
            HANDLE_EXPRCASE(Or)
            HANDLE_EXPRCASE(Xor)
            HANDLE_EXPRCASE(Eq)
            HANDLE_EXPRCASE(NotEq)
            HANDLE_EXPRCASE(SLt)
            HANDLE_EXPRCASE(SLtEq)
            HANDLE_EXPRCASE(SGt)
            HANDLE_EXPRCASE(SGtEq)
            HANDLE_EXPRCASE(ULt)
            HANDLE_EXPRCASE(ULtEq)
            HANDLE_EXPRCASE(UGt)
            HANDLE_EXPRCASE(UGtEq)
            HANDLE_EXPRCASE(Select)
            HANDLE_EXPRCASE(ArrayRead)
            HANDLE_EXPRCASE(ArrayWrite)
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
    virtual ReturnT visitUndef(const std::shared_ptr<UndefExpr>& expr) {
        return this->visitExpr(expr);
    }
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
    virtual ReturnT visitZExt(const std::shared_ptr<ZExtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitSExt(const std::shared_ptr<SExtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitExtract(const std::shared_ptr<ExtractExpr>& expr) {
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
    virtual ReturnT visitShl(const std::shared_ptr<ShlExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitLShr(const std::shared_ptr<LShrExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitAShr(const std::shared_ptr<AShrExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitBAnd(const std::shared_ptr<BAndExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitBOr(const std::shared_ptr<BOrExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitBXor(const std::shared_ptr<BXorExpr>& expr) {
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
    
    virtual ReturnT visitSLt(const std::shared_ptr<SLtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitSLtEq(const std::shared_ptr<SLtEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitSGt(const std::shared_ptr<SGtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitSGtEq(const std::shared_ptr<SGtEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    virtual ReturnT visitULt(const std::shared_ptr<ULtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitULtEq(const std::shared_ptr<ULtEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitUGt(const std::shared_ptr<UGtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitUGtEq(const std::shared_ptr<UGtEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Ternary
    virtual ReturnT visitSelect(const std::shared_ptr<SelectExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Arrays
    virtual ReturnT visitArrayRead(const std::shared_ptr<ArrayReadExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    virtual ReturnT visitArrayWrite(const std::shared_ptr<ArrayWriteExpr>& expr) {
        return this->visitNonNullary(expr);
    }
};

} // end namespace gazer

#endif
