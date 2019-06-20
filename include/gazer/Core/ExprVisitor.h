#ifndef _GAZER_CORE_EXPRVISITOR_H
#define _GAZER_CORE_EXPRVISITOR_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Variable.h"

namespace gazer
{

/// Generic visitor interface for expressions.
template<class ReturnT = void>
class ExprVisitor
{
public:
    /// Handler for expression pointers.
    virtual ReturnT visit(const ExprPtr& expr) {
        #define GAZER_EXPR_KIND(KIND)                                       \
            case Expr::KIND:                                                \
                return this->visit##KIND(llvm::cast<KIND##Expr>(expr));     \

        switch (expr->getKind()) {
            #include "gazer/Core/Expr/ExprKind.inc"
        }

        llvm_unreachable("Unknown expression kind");

        #undef GAZER_EXPR_KIND
    }

    virtual ~ExprVisitor() {}

protected:

    //--- Generic fallback methods ---//
    // In case you don't override specific expression classes,
    // these will be called as a fallback.
    
    /// Basic fallback method for unhandled instruction types.
    virtual ReturnT visitExpr(const ExprPtr& expr) = 0;

    virtual ReturnT visitNonNullary(const ExprRef<NonNullaryExpr>& expr) {
        return visitExpr(expr);
    }

    // Nullary
    virtual ReturnT visitUndef(const ExprRef<UndefExpr>& expr) {
        return this->visitExpr(expr);
    }
    virtual ReturnT visitLiteral(const ExprRef<LiteralExpr>& expr) {
        return this->visitExpr(expr);
    }
    virtual ReturnT visitVarRef(const ExprRef<VarRefExpr>& expr) {
        return this->visitExpr(expr);
    }

    // Unary
    virtual ReturnT visitNot(const ExprRef<NotExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitZExt(const ExprRef<ZExtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitSExt(const ExprRef<SExtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitExtract(const ExprRef<ExtractExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Binary
    virtual ReturnT visitAdd(const ExprRef<AddExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitSub(const ExprRef<SubExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitMul(const ExprRef<MulExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    virtual ReturnT visitSDiv(const ExprRef<SDivExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitUDiv(const ExprRef<UDivExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitSRem(const ExprRef<SRemExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitURem(const ExprRef<URemExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    virtual ReturnT visitShl(const ExprRef<ShlExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitLShr(const ExprRef<LShrExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitAShr(const ExprRef<AShrExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitBAnd(const ExprRef<BAndExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitBOr(const ExprRef<BOrExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitBXor(const ExprRef<BXorExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Logic
    virtual ReturnT visitAnd(const ExprRef<AndExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitOr(const ExprRef<OrExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitXor(const ExprRef<XorExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitImply(const ExprRef<ImplyExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Compare
    virtual ReturnT visitEq(const ExprRef<EqExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitNotEq(const ExprRef<NotEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    
    virtual ReturnT visitSLt(const ExprRef<SLtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitSLtEq(const ExprRef<SLtEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitSGt(const ExprRef<SGtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitSGtEq(const ExprRef<SGtEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    virtual ReturnT visitULt(const ExprRef<ULtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitULtEq(const ExprRef<ULtEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitUGt(const ExprRef<UGtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitUGtEq(const ExprRef<UGtEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Floating-point queries
    virtual ReturnT visitFIsNan(const ExprRef<FIsNanExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitFIsInf(const ExprRef<FIsInfExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Floating-point arithmetic
    virtual ReturnT visitFAdd(const ExprRef<FAddExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitFSub(const ExprRef<FSubExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitFMul(const ExprRef<FMulExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitFDiv(const ExprRef<FDivExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Floating-point compare
    virtual ReturnT visitFEq(const ExprRef<FEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitFGt(const ExprRef<FGtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitFGtEq(const ExprRef<FGtEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitFLt(const ExprRef<FLtExpr>& expr) {
        return this->visitNonNullary(expr);
    }
    virtual ReturnT visitFLtEq(const ExprRef<FLtEqExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Ternary
    virtual ReturnT visitSelect(const ExprRef<SelectExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    // Arrays
    virtual ReturnT visitArrayRead(const ExprRef<ArrayReadExpr>& expr) {
        return this->visitNonNullary(expr);
    }

    virtual ReturnT visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr) {
        return this->visitNonNullary(expr);
    }
};

} // end namespace gazer

#endif
