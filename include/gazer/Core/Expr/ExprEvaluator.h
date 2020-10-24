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
class ExprEvaluatorBase : public ExprEvaluator, private ExprWalker<ExprEvaluatorBase, ExprRef<AtomicExpr>>
{
    friend class ExprWalker<ExprEvaluatorBase, ExprRef<AtomicExpr>>;
public:
    ExprRef<AtomicExpr> evaluate(const ExprPtr& expr) override {
        return this->walk(expr);
    }

protected:
    virtual ExprRef<AtomicExpr> getVariableValue(Variable& variable) = 0;

private:
    ExprRef<AtomicExpr> visitExpr(const ExprPtr& expr);

    // Nullary
    ExprRef<AtomicExpr> visitUndef(const ExprRef<UndefExpr>& expr);
    ExprRef<AtomicExpr> visitLiteral(const ExprRef<AtomicExpr>& expr);
    ExprRef<AtomicExpr> visitVarRef(const ExprRef<VarRefExpr>& expr);

    // Unary
    ExprRef<AtomicExpr> visitNot(const ExprRef<NotExpr>& expr);
    ExprRef<AtomicExpr> visitZExt(const ExprRef<ZExtExpr>& expr);
    ExprRef<AtomicExpr> visitSExt(const ExprRef<SExtExpr>& expr);
    ExprRef<AtomicExpr> visitExtract(const ExprRef<ExtractExpr>& expr);
    ExprRef<AtomicExpr> visitBvConcat(const ExprRef<BvConcatExpr>& expr);

    // Binary
    ExprRef<AtomicExpr> visitAdd(const ExprRef<AddExpr>& expr);
    ExprRef<AtomicExpr> visitSub(const ExprRef<SubExpr>& expr);
    ExprRef<AtomicExpr> visitMul(const ExprRef<MulExpr>& expr);
    ExprRef<AtomicExpr> visitDiv(const ExprRef<DivExpr>& expr);
    ExprRef<AtomicExpr> visitMod(const ExprRef<ModExpr>& expr);
    ExprRef<AtomicExpr> visitRem(const ExprRef<RemExpr>& expr);

    ExprRef<AtomicExpr> visitBvSDiv(const ExprRef<BvSDivExpr>& expr);
    ExprRef<AtomicExpr> visitBvUDiv(const ExprRef<BvUDivExpr>& expr);
    ExprRef<AtomicExpr> visitBvSRem(const ExprRef<BvSRemExpr>& expr);
    ExprRef<AtomicExpr> visitBvURem(const ExprRef<BvURemExpr>& expr);

    ExprRef<AtomicExpr> visitShl(const ExprRef<ShlExpr>& expr);
    ExprRef<AtomicExpr> visitLShr(const ExprRef<LShrExpr>& expr);
    ExprRef<AtomicExpr> visitAShr(const ExprRef<AShrExpr>& expr);
    ExprRef<AtomicExpr> visitBvAnd(const ExprRef<BvAndExpr>& expr);
    ExprRef<AtomicExpr> visitBvOr(const ExprRef<BvOrExpr>& expr);
    ExprRef<AtomicExpr> visitBvXor(const ExprRef<BvXorExpr>& expr);

    // Logic
    ExprRef<AtomicExpr> visitAnd(const ExprRef<AndExpr>& expr);
    ExprRef<AtomicExpr> visitOr(const ExprRef<OrExpr>& expr);
    ExprRef<AtomicExpr> visitImply(const ExprRef<ImplyExpr>& expr);

    // Compare
    ExprRef<AtomicExpr> visitEq(const ExprRef<EqExpr>& expr);
    ExprRef<AtomicExpr> visitNotEq(const ExprRef<NotEqExpr>& expr);

    ExprRef<AtomicExpr> visitLt(const ExprRef<LtExpr>& expr);
    ExprRef<AtomicExpr> visitLtEq(const ExprRef<LtEqExpr>& expr);
    ExprRef<AtomicExpr> visitGt(const ExprRef<GtExpr>& expr);
    ExprRef<AtomicExpr> visitGtEq(const ExprRef<GtEqExpr>& expr);

    ExprRef<AtomicExpr> visitBvSLt(const ExprRef<BvSLtExpr>& expr);
    ExprRef<AtomicExpr> visitBvSLtEq(const ExprRef<BvSLtEqExpr>& expr);
    ExprRef<AtomicExpr> visitBvSGt(const ExprRef<BvSGtExpr>& expr);
    ExprRef<AtomicExpr> visitBvSGtEq(const ExprRef<BvSGtEqExpr>& expr);

    ExprRef<AtomicExpr> visitBvULt(const ExprRef<BvULtExpr>& expr);
    ExprRef<AtomicExpr> visitBvULtEq(const ExprRef<BvULtEqExpr>& expr);
    ExprRef<AtomicExpr> visitBvUGt(const ExprRef<BvUGtExpr>& expr);
    ExprRef<AtomicExpr> visitBvUGtEq(const ExprRef<BvUGtEqExpr>& expr);

    // Floating-point queries
    ExprRef<AtomicExpr> visitFIsNan(const ExprRef<FIsNanExpr>& expr);
    ExprRef<AtomicExpr> visitFIsInf(const ExprRef<FIsInfExpr>& expr);

    // Floating-point arithmetic
    ExprRef<AtomicExpr> visitFAdd(const ExprRef<FAddExpr>& expr);
    ExprRef<AtomicExpr> visitFSub(const ExprRef<FSubExpr>& expr);
    ExprRef<AtomicExpr> visitFMul(const ExprRef<FMulExpr>& expr);
    ExprRef<AtomicExpr> visitFDiv(const ExprRef<FDivExpr>& expr);

    // Floating-point compare
    ExprRef<AtomicExpr> visitFEq(const ExprRef<FEqExpr>& expr);
    ExprRef<AtomicExpr> visitFGt(const ExprRef<FGtExpr>& expr);
    ExprRef<AtomicExpr> visitFGtEq(const ExprRef<FGtEqExpr>& expr);
    ExprRef<AtomicExpr> visitFLt(const ExprRef<FLtExpr>& expr);
    ExprRef<AtomicExpr> visitFLtEq(const ExprRef<FLtEqExpr>& expr);


    // Floating-point casts
    ExprRef<AtomicExpr> visitFCast(const ExprRef<FCastExpr>& expr);
    ExprRef<AtomicExpr> visitSignedToFp(const ExprRef<SignedToFpExpr>& expr);
    ExprRef<AtomicExpr> visitUnsignedToFp(const ExprRef<UnsignedToFpExpr>& expr);
    ExprRef<AtomicExpr> visitFpToSigned(const ExprRef<FpToSignedExpr>& expr);
    ExprRef<AtomicExpr> visitFpToUnsigned(const ExprRef<FpToUnsignedExpr>& expr);
    ExprRef<AtomicExpr> visitFpToBv(const ExprRef<FpToBvExpr>& expr);
    ExprRef<AtomicExpr> visitBvToFp(const ExprRef<BvToFpExpr>& expr);

    // Ternary
    ExprRef<AtomicExpr> visitSelect(const ExprRef<SelectExpr>& expr);
    // Arrays
    ExprRef<AtomicExpr> visitArrayRead(const ExprRef<ArrayReadExpr>& expr);
    ExprRef<AtomicExpr> visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr);
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

}

#endif