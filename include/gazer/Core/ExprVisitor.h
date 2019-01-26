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
                return this->visit##KIND(llvm::cast<KIND##Expr>(expr));     \

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
            HANDLE_EXPRCASE(SDiv)
            HANDLE_EXPRCASE(UDiv)
            HANDLE_EXPRCASE(SRem)
            HANDLE_EXPRCASE(URem)
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
            HANDLE_EXPRCASE(FIsNan)
            HANDLE_EXPRCASE(FIsInf)
            HANDLE_EXPRCASE(FAdd)
            HANDLE_EXPRCASE(FSub)
            HANDLE_EXPRCASE(FMul)
            HANDLE_EXPRCASE(FDiv)
            HANDLE_EXPRCASE(FEq)
            HANDLE_EXPRCASE(FGt)
            HANDLE_EXPRCASE(FGtEq)
            HANDLE_EXPRCASE(FLt)
            HANDLE_EXPRCASE(FLtEq)
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
