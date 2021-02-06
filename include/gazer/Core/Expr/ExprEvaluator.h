//==- ExprEvaluator.h - Expression evaluation -------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#ifndef GAZER_CORE_EXPR_EXPREVALUATOR_H
#define GAZER_CORE_EXPR_EXPREVALUATOR_H

#include "gazer/Core/Expr/ExprWalker.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/Valuation.h"

namespace gazer
{

/// Interface for evaluating expressions.
class ExprEvaluator
{
public:
    virtual ExprRef<AtomicExpr> evaluate(const ExprPtr& expr) = 0;
};

/// Base class for expression evaluation implementations.
/// This abstract class provides all methods to evaluate an expression, except for
/// the means of acquiring the value of a variable.
class ExprEvaluatorBase : public ExprEvaluator, public ExprWalker<ExprRef<AtomicExpr>>
{
public:
    ExprRef<AtomicExpr> evaluate(const ExprPtr& expr) override {
        return this->walk(expr);
    }

protected:
    virtual ExprRef<AtomicExpr> getVariableValue(Variable& variable) = 0;

private:
    ExprRef<AtomicExpr> visitExpr(const ExprPtr& expr) override;

    // Nullary
    ExprRef<AtomicExpr> visitUndef(const ExprRef<UndefExpr>& expr) override;
    ExprRef<AtomicExpr> visitLiteral(const ExprRef<LiteralExpr>& expr) override;
    ExprRef<AtomicExpr> visitVarRef(const ExprRef<VarRefExpr>& expr) override;

    // Unary
    ExprRef<AtomicExpr> visitNot(const ExprRef<NotExpr>& expr) override;
    ExprRef<AtomicExpr> visitZExt(const ExprRef<ZExtExpr>& expr) override;
    ExprRef<AtomicExpr> visitSExt(const ExprRef<SExtExpr>& expr) override;
    ExprRef<AtomicExpr> visitExtract(const ExprRef<ExtractExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvConcat(const ExprRef<BvConcatExpr>& expr) override;

    // Binary
    ExprRef<AtomicExpr> visitAdd(const ExprRef<AddExpr>& expr) override;
    ExprRef<AtomicExpr> visitSub(const ExprRef<SubExpr>& expr) override;
    ExprRef<AtomicExpr> visitMul(const ExprRef<MulExpr>& expr) override;
    ExprRef<AtomicExpr> visitDiv(const ExprRef<DivExpr>& expr) override;
    ExprRef<AtomicExpr> visitMod(const ExprRef<ModExpr>& expr) override;
    ExprRef<AtomicExpr> visitRem(const ExprRef<RemExpr>& expr) override;

    ExprRef<AtomicExpr> visitBvSDiv(const ExprRef<BvSDivExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvUDiv(const ExprRef<BvUDivExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvSRem(const ExprRef<BvSRemExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvURem(const ExprRef<BvURemExpr>& expr) override;

    ExprRef<AtomicExpr> visitShl(const ExprRef<ShlExpr>& expr) override;
    ExprRef<AtomicExpr> visitLShr(const ExprRef<LShrExpr>& expr) override;
    ExprRef<AtomicExpr> visitAShr(const ExprRef<AShrExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvAnd(const ExprRef<BvAndExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvOr(const ExprRef<BvOrExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvXor(const ExprRef<BvXorExpr>& expr) override;

    // Logic
    ExprRef<AtomicExpr> visitAnd(const ExprRef<AndExpr>& expr) override;
    ExprRef<AtomicExpr> visitOr(const ExprRef<OrExpr>& expr) override;
    ExprRef<AtomicExpr> visitImply(const ExprRef<ImplyExpr>& expr) override;

    // Compare
    ExprRef<AtomicExpr> visitEq(const ExprRef<EqExpr>& expr) override;
    ExprRef<AtomicExpr> visitNotEq(const ExprRef<NotEqExpr>& expr) override;

    ExprRef<AtomicExpr> visitLt(const ExprRef<LtExpr>& expr) override;
    ExprRef<AtomicExpr> visitLtEq(const ExprRef<LtEqExpr>& expr) override;
    ExprRef<AtomicExpr> visitGt(const ExprRef<GtExpr>& expr) override;
    ExprRef<AtomicExpr> visitGtEq(const ExprRef<GtEqExpr>& expr) override;

    ExprRef<AtomicExpr> visitBvSLt(const ExprRef<BvSLtExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvSLtEq(const ExprRef<BvSLtEqExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvSGt(const ExprRef<BvSGtExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvSGtEq(const ExprRef<BvSGtEqExpr>& expr) override;

    ExprRef<AtomicExpr> visitBvULt(const ExprRef<BvULtExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvULtEq(const ExprRef<BvULtEqExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvUGt(const ExprRef<BvUGtExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvUGtEq(const ExprRef<BvUGtEqExpr>& expr) override;

    // Floating-point queries
    ExprRef<AtomicExpr> visitFIsNan(const ExprRef<FIsNanExpr>& expr) override;
    ExprRef<AtomicExpr> visitFIsInf(const ExprRef<FIsInfExpr>& expr) override;

    // Floating-point arithmetic
    ExprRef<AtomicExpr> visitFAdd(const ExprRef<FAddExpr>& expr) override;
    ExprRef<AtomicExpr> visitFSub(const ExprRef<FSubExpr>& expr) override;
    ExprRef<AtomicExpr> visitFMul(const ExprRef<FMulExpr>& expr) override;
    ExprRef<AtomicExpr> visitFDiv(const ExprRef<FDivExpr>& expr) override;

    // Floating-point compare
    ExprRef<AtomicExpr> visitFEq(const ExprRef<FEqExpr>& expr) override;
    ExprRef<AtomicExpr> visitFGt(const ExprRef<FGtExpr>& expr) override;
    ExprRef<AtomicExpr> visitFGtEq(const ExprRef<FGtEqExpr>& expr) override;
    ExprRef<AtomicExpr> visitFLt(const ExprRef<FLtExpr>& expr) override;
    ExprRef<AtomicExpr> visitFLtEq(const ExprRef<FLtEqExpr>& expr) override;


    // Floating-point casts
    ExprRef<AtomicExpr> visitFCast(const ExprRef<FCastExpr>& expr) override;
    ExprRef<AtomicExpr> visitSignedToFp(const ExprRef<SignedToFpExpr>& expr) override;
    ExprRef<AtomicExpr> visitUnsignedToFp(const ExprRef<UnsignedToFpExpr>& expr) override;
    ExprRef<AtomicExpr> visitFpToSigned(const ExprRef<FpToSignedExpr>& expr) override;
    ExprRef<AtomicExpr> visitFpToUnsigned(const ExprRef<FpToUnsignedExpr>& expr) override;
    ExprRef<AtomicExpr> visitFpToBv(const ExprRef<FpToBvExpr>& expr) override;
    ExprRef<AtomicExpr> visitBvToFp(const ExprRef<BvToFpExpr>& expr) override;

    // Ternary
    ExprRef<AtomicExpr> visitSelect(const ExprRef<SelectExpr>& expr) override;
    // Arrays
    ExprRef<AtomicExpr> visitArrayRead(const ExprRef<ArrayReadExpr>& expr) override;
    ExprRef<AtomicExpr> visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr) override;
};

/// Evaluates expressions based on a Valuation object
class ValuationExprEvaluator : public ExprEvaluatorBase
{
public:
    explicit ValuationExprEvaluator(const Valuation& valuation)
        : mValuation(valuation)
    {}

protected:
    ExprRef<AtomicExpr> getVariableValue(Variable& variable) override;

private:
    const Valuation& mValuation;
};

} // namespace gazer

#endif