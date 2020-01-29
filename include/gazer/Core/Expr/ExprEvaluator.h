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

/// Base class for expression evaluation implementations.
/// This abstract class provides all methods to evaluate an expression, except for
/// the means of acquiring the value of a variable.
class ExprEvaluatorBase : public ExprWalker<ExprEvaluatorBase, ExprRef<LiteralExpr>>
{
    friend class ExprWalker<ExprEvaluatorBase, ExprRef<LiteralExpr>>;
private:
    ExprRef<LiteralExpr> visitExpr(const ExprPtr& expr);

    // Nullary
    ExprRef<LiteralExpr> visitUndef(const ExprRef<UndefExpr>& expr);
    ExprRef<LiteralExpr> visitLiteral(const ExprRef<LiteralExpr>& expr);
    ExprRef<LiteralExpr> visitVarRef(const ExprRef<VarRefExpr>& expr);

    // Unary
    ExprRef<LiteralExpr> visitNot(const ExprRef<NotExpr>& expr);
    ExprRef<LiteralExpr> visitZExt(const ExprRef<ZExtExpr>& expr);
    ExprRef<LiteralExpr> visitSExt(const ExprRef<SExtExpr>& expr);
    ExprRef<LiteralExpr> visitExtract(const ExprRef<ExtractExpr>& expr);

    // Binary
    ExprRef<LiteralExpr> visitAdd(const ExprRef<AddExpr>& expr);
    ExprRef<LiteralExpr> visitSub(const ExprRef<SubExpr>& expr);
    ExprRef<LiteralExpr> visitMul(const ExprRef<MulExpr>& expr);
    ExprRef<LiteralExpr> visitDiv(const ExprRef<DivExpr>& expr);
    ExprRef<LiteralExpr> visitMod(const ExprRef<ModExpr>& expr);
    ExprRef<LiteralExpr> visitRem(const ExprRef<RemExpr>& expr);

    ExprRef<LiteralExpr> visitBvSDiv(const ExprRef<BvSDivExpr>& expr);
    ExprRef<LiteralExpr> visitBvUDiv(const ExprRef<BvUDivExpr>& expr);
    ExprRef<LiteralExpr> visitBvSRem(const ExprRef<BvSRemExpr>& expr);
    ExprRef<LiteralExpr> visitBvURem(const ExprRef<BvURemExpr>& expr);

    ExprRef<LiteralExpr> visitShl(const ExprRef<ShlExpr>& expr);
    ExprRef<LiteralExpr> visitLShr(const ExprRef<LShrExpr>& expr);
    ExprRef<LiteralExpr> visitAShr(const ExprRef<AShrExpr>& expr);
    ExprRef<LiteralExpr> visitBvAnd(const ExprRef<BvAndExpr>& expr);
    ExprRef<LiteralExpr> visitBvOr(const ExprRef<BvOrExpr>& expr);
    ExprRef<LiteralExpr> visitBvXor(const ExprRef<BvXorExpr>& expr);

    // Logic
    ExprRef<LiteralExpr> visitAnd(const ExprRef<AndExpr>& expr);
    ExprRef<LiteralExpr> visitOr(const ExprRef<OrExpr>& expr);
    ExprRef<LiteralExpr> visitImply(const ExprRef<ImplyExpr>& expr);

    // Compare
    ExprRef<LiteralExpr> visitEq(const ExprRef<EqExpr>& expr);
    ExprRef<LiteralExpr> visitNotEq(const ExprRef<NotEqExpr>& expr);

    ExprRef<LiteralExpr> visitLt(const ExprRef<LtExpr>& expr);
    ExprRef<LiteralExpr> visitLtEq(const ExprRef<LtEqExpr>& expr);
    ExprRef<LiteralExpr> visitGt(const ExprRef<GtExpr>& expr);
    ExprRef<LiteralExpr> visitGtEq(const ExprRef<GtEqExpr>& expr);

    ExprRef<LiteralExpr> visitBvSLt(const ExprRef<BvSLtExpr>& expr);
    ExprRef<LiteralExpr> visitBvSLtEq(const ExprRef<BvSLtEqExpr>& expr);
    ExprRef<LiteralExpr> visitBvSGt(const ExprRef<BvSGtExpr>& expr);
    ExprRef<LiteralExpr> visitBvSGtEq(const ExprRef<BvSGtEqExpr>& expr);

    ExprRef<LiteralExpr> visitBvULt(const ExprRef<BvULtExpr>& expr);
    ExprRef<LiteralExpr> visitBvULtEq(const ExprRef<BvULtEqExpr>& expr);
    ExprRef<LiteralExpr> visitBvUGt(const ExprRef<BvUGtExpr>& expr);
    ExprRef<LiteralExpr> visitBvUGtEq(const ExprRef<BvUGtEqExpr>& expr);

    // Floating-point queries
    ExprRef<LiteralExpr> visitFIsNan(const ExprRef<FIsNanExpr>& expr);
    ExprRef<LiteralExpr> visitFIsInf(const ExprRef<FIsInfExpr>& expr);

    // Floating-point arithmetic
    ExprRef<LiteralExpr> visitFAdd(const ExprRef<FAddExpr>& expr);
    ExprRef<LiteralExpr> visitFSub(const ExprRef<FSubExpr>& expr);
    ExprRef<LiteralExpr> visitFMul(const ExprRef<FMulExpr>& expr);
    ExprRef<LiteralExpr> visitFDiv(const ExprRef<FDivExpr>& expr);

    // Floating-point compare
    ExprRef<LiteralExpr> visitFEq(const ExprRef<FEqExpr>& expr);
    ExprRef<LiteralExpr> visitFGt(const ExprRef<FGtExpr>& expr);
    ExprRef<LiteralExpr> visitFGtEq(const ExprRef<FGtEqExpr>& expr);
    ExprRef<LiteralExpr> visitFLt(const ExprRef<FLtExpr>& expr);
    ExprRef<LiteralExpr> visitFLtEq(const ExprRef<FLtEqExpr>& expr);
    // Ternary
    ExprRef<LiteralExpr> visitSelect(const ExprRef<SelectExpr>& expr);
    // Arrays
    ExprRef<LiteralExpr> visitArrayRead(const ExprRef<ArrayReadExpr>& expr);
    ExprRef<LiteralExpr> visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr);

protected:
    virtual ExprRef<LiteralExpr> getVariableValue(Variable& variable) = 0;

};

/// Evaluates expressions based on a Valuation object
class ExprEvaluator : public ExprEvaluatorBase
{
public:
    explicit ExprEvaluator(Valuation valuation)
        : mValuation(std::move(valuation))
    {}

protected:
    ExprRef<LiteralExpr> getVariableValue(Variable& variable) override {
        return mValuation[variable];
    }

private:
    Valuation mValuation;
};

}

#endif