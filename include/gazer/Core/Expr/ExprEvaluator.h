#ifndef _GAZER_CORE_EXPR_EXPREVALUATOR_H
#define _GAZER_CORE_EXPR_EXPREVALUATOR_H

#include "gazer/Core/ExprVisitor.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/Valuation.h"

namespace gazer
{

/// Base class for expression evaluation implementations.
/// This class provides all methods to evaluate an expression, except for
/// the means of acquiring the value of a variable.
class ExprEvaluatorBase : public ExprVisitor<std::shared_ptr<LiteralExpr>>
{
public:

protected:
    std::shared_ptr<LiteralExpr> visitExpr(const ExprPtr& expr) override;

    // Nullary
    std::shared_ptr<LiteralExpr> visitUndef(const std::shared_ptr<UndefExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitLiteral(const std::shared_ptr<LiteralExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitVarRef(const std::shared_ptr<VarRefExpr>& expr) override;

    // Unary
    std::shared_ptr<LiteralExpr> visitNot(const std::shared_ptr<NotExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitZExt(const std::shared_ptr<ZExtExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitSExt(const std::shared_ptr<SExtExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitExtract(const std::shared_ptr<ExtractExpr>& expr) override;

    // Binary
    std::shared_ptr<LiteralExpr> visitAdd(const std::shared_ptr<AddExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitSub(const std::shared_ptr<SubExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitMul(const std::shared_ptr<MulExpr>& expr) override;

    std::shared_ptr<LiteralExpr> visitSDiv(const std::shared_ptr<SDivExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitUDiv(const std::shared_ptr<UDivExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitSRem(const std::shared_ptr<SRemExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitURem(const std::shared_ptr<URemExpr>& expr) override;

    std::shared_ptr<LiteralExpr> visitShl(const std::shared_ptr<ShlExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitLShr(const std::shared_ptr<LShrExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitAShr(const std::shared_ptr<AShrExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitBAnd(const std::shared_ptr<BAndExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitBOr(const std::shared_ptr<BOrExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitBXor(const std::shared_ptr<BXorExpr>& expr) override;

    // Logic
    std::shared_ptr<LiteralExpr> visitAnd(const std::shared_ptr<AndExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitOr(const std::shared_ptr<OrExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitXor(const std::shared_ptr<XorExpr>& expr) override;

    // Compare
    std::shared_ptr<LiteralExpr> visitEq(const std::shared_ptr<EqExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitNotEq(const std::shared_ptr<NotEqExpr>& expr) override;
    
    std::shared_ptr<LiteralExpr> visitSLt(const std::shared_ptr<SLtExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitSLtEq(const std::shared_ptr<SLtEqExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitSGt(const std::shared_ptr<SGtExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitSGtEq(const std::shared_ptr<SGtEqExpr>& expr) override;

    std::shared_ptr<LiteralExpr> visitULt(const std::shared_ptr<ULtExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitULtEq(const std::shared_ptr<ULtEqExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitUGt(const std::shared_ptr<UGtExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitUGtEq(const std::shared_ptr<UGtEqExpr>& expr) override;

    // Floating-point queries
    std::shared_ptr<LiteralExpr> visitFIsNan(const std::shared_ptr<FIsNanExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitFIsInf(const std::shared_ptr<FIsInfExpr>& expr) override;

    // Floating-point arithmetic
    std::shared_ptr<LiteralExpr> visitFAdd(const std::shared_ptr<FAddExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitFSub(const std::shared_ptr<FSubExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitFMul(const std::shared_ptr<FMulExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitFDiv(const std::shared_ptr<FDivExpr>& expr) override;

    // Floating-point compare
    std::shared_ptr<LiteralExpr> visitFEq(const std::shared_ptr<FEqExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitFGt(const std::shared_ptr<FGtExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitFGtEq(const std::shared_ptr<FGtEqExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitFLt(const std::shared_ptr<FLtExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitFLtEq(const std::shared_ptr<FLtEqExpr>& expr) override;
    // Ternary
    std::shared_ptr<LiteralExpr> visitSelect(const std::shared_ptr<SelectExpr>& expr) override;
    // Arrays
    std::shared_ptr<LiteralExpr> visitArrayRead(const std::shared_ptr<ArrayReadExpr>& expr) override;
    std::shared_ptr<LiteralExpr> visitArrayWrite(const std::shared_ptr<ArrayWriteExpr>& expr) override;

protected:
    virtual std::shared_ptr<LiteralExpr> getVariableValue(const Variable& variable) = 0;

};

/// Evaluates expressions based on a Valuation object
class ExprEvaluator : public ExprEvaluatorBase
{
public:
    ExprEvaluator(Valuation& valuation)
        : mValuation(valuation)
    {}

protected:
    std::shared_ptr<LiteralExpr> getVariableValue(const Variable& variable) override {
        return mValuation[variable];
    }

private:
    Valuation& mValuation;
};

}

#endif
