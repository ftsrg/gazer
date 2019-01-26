#ifndef _GAZER_CORE_EXPR_EXPREVALUATOR_H
#define _GAZER_CORE_EXPR_EXPREVALUATOR_H

#include "gazer/Core/ExprVisitor.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/Valuation.h"

namespace gazer
{

/// Base class for expression evaluation implementations.
/// This abstract class provides all methods to evaluate an expression, except for
/// the means of acquiring the value of a variable.
class ExprEvaluatorBase : public ExprVisitor<ExprRef<LiteralExpr>>
{
public:

protected:
    ExprRef<LiteralExpr> visitExpr(const ExprPtr& expr) override;

    // Nullary
    ExprRef<LiteralExpr> visitUndef(const ExprRef<UndefExpr>& expr) override;
    ExprRef<LiteralExpr> visitLiteral(const ExprRef<LiteralExpr>& expr) override;
    ExprRef<LiteralExpr> visitVarRef(const ExprRef<VarRefExpr>& expr) override;

    // Unary
    ExprRef<LiteralExpr> visitNot(const ExprRef<NotExpr>& expr) override;
    ExprRef<LiteralExpr> visitZExt(const ExprRef<ZExtExpr>& expr) override;
    ExprRef<LiteralExpr> visitSExt(const ExprRef<SExtExpr>& expr) override;
    ExprRef<LiteralExpr> visitExtract(const ExprRef<ExtractExpr>& expr) override;

    // Binary
    ExprRef<LiteralExpr> visitAdd(const ExprRef<AddExpr>& expr) override;
    ExprRef<LiteralExpr> visitSub(const ExprRef<SubExpr>& expr) override;
    ExprRef<LiteralExpr> visitMul(const ExprRef<MulExpr>& expr) override;

    ExprRef<LiteralExpr> visitSDiv(const ExprRef<SDivExpr>& expr) override;
    ExprRef<LiteralExpr> visitUDiv(const ExprRef<UDivExpr>& expr) override;
    ExprRef<LiteralExpr> visitSRem(const ExprRef<SRemExpr>& expr) override;
    ExprRef<LiteralExpr> visitURem(const ExprRef<URemExpr>& expr) override;

    ExprRef<LiteralExpr> visitShl(const ExprRef<ShlExpr>& expr) override;
    ExprRef<LiteralExpr> visitLShr(const ExprRef<LShrExpr>& expr) override;
    ExprRef<LiteralExpr> visitAShr(const ExprRef<AShrExpr>& expr) override;
    ExprRef<LiteralExpr> visitBAnd(const ExprRef<BAndExpr>& expr) override;
    ExprRef<LiteralExpr> visitBOr(const ExprRef<BOrExpr>& expr) override;
    ExprRef<LiteralExpr> visitBXor(const ExprRef<BXorExpr>& expr) override;

    // Logic
    ExprRef<LiteralExpr> visitAnd(const ExprRef<AndExpr>& expr) override;
    ExprRef<LiteralExpr> visitOr(const ExprRef<OrExpr>& expr) override;
    ExprRef<LiteralExpr> visitXor(const ExprRef<XorExpr>& expr) override;

    // Compare
    ExprRef<LiteralExpr> visitEq(const ExprRef<EqExpr>& expr) override;
    ExprRef<LiteralExpr> visitNotEq(const ExprRef<NotEqExpr>& expr) override;
    
    ExprRef<LiteralExpr> visitSLt(const ExprRef<SLtExpr>& expr) override;
    ExprRef<LiteralExpr> visitSLtEq(const ExprRef<SLtEqExpr>& expr) override;
    ExprRef<LiteralExpr> visitSGt(const ExprRef<SGtExpr>& expr) override;
    ExprRef<LiteralExpr> visitSGtEq(const ExprRef<SGtEqExpr>& expr) override;

    ExprRef<LiteralExpr> visitULt(const ExprRef<ULtExpr>& expr) override;
    ExprRef<LiteralExpr> visitULtEq(const ExprRef<ULtEqExpr>& expr) override;
    ExprRef<LiteralExpr> visitUGt(const ExprRef<UGtExpr>& expr) override;
    ExprRef<LiteralExpr> visitUGtEq(const ExprRef<UGtEqExpr>& expr) override;

    // Floating-point queries
    ExprRef<LiteralExpr> visitFIsNan(const ExprRef<FIsNanExpr>& expr) override;
    ExprRef<LiteralExpr> visitFIsInf(const ExprRef<FIsInfExpr>& expr) override;

    // Floating-point arithmetic
    ExprRef<LiteralExpr> visitFAdd(const ExprRef<FAddExpr>& expr) override;
    ExprRef<LiteralExpr> visitFSub(const ExprRef<FSubExpr>& expr) override;
    ExprRef<LiteralExpr> visitFMul(const ExprRef<FMulExpr>& expr) override;
    ExprRef<LiteralExpr> visitFDiv(const ExprRef<FDivExpr>& expr) override;

    // Floating-point compare
    ExprRef<LiteralExpr> visitFEq(const ExprRef<FEqExpr>& expr) override;
    ExprRef<LiteralExpr> visitFGt(const ExprRef<FGtExpr>& expr) override;
    ExprRef<LiteralExpr> visitFGtEq(const ExprRef<FGtEqExpr>& expr) override;
    ExprRef<LiteralExpr> visitFLt(const ExprRef<FLtExpr>& expr) override;
    ExprRef<LiteralExpr> visitFLtEq(const ExprRef<FLtEqExpr>& expr) override;
    // Ternary
    ExprRef<LiteralExpr> visitSelect(const ExprRef<SelectExpr>& expr) override;
    // Arrays
    ExprRef<LiteralExpr> visitArrayRead(const ExprRef<ArrayReadExpr>& expr) override;
    ExprRef<LiteralExpr> visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr) override;

protected:
    virtual ExprRef<LiteralExpr> getVariableValue(const Variable& variable) = 0;

};

/// Evaluates expressions based on a Valuation object
class ExprEvaluator : public ExprEvaluatorBase
{
public:
    ExprEvaluator(Valuation& valuation)
        : mValuation(valuation)
    {}

protected:
    ExprRef<LiteralExpr> getVariableValue(const Variable& variable) override {
        return mValuation[variable];
    }

private:
    Valuation& mValuation;
};

}

#endif
