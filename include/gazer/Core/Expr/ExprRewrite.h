//==- ExprRewrite.h ---------------------------------------------*- C++ -*--==//
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
#ifndef GAZER_CORE_EXPR_EXPRREWRITE_H
#define GAZER_CORE_EXPR_EXPRREWRITE_H

#include "gazer/Core/Expr/ExprWalker.h"
#include "gazer/Core/Expr/ExprBuilder.h"

namespace gazer
{


/// Base class for expression rewrite implementations.
class ExprRewrite : public ExprWalker<ExprPtr>
{
public:
    explicit ExprRewrite(ExprBuilder& builder)
        : mExprBuilder(builder)
    {}

    ExprPtr rewrite(const ExprPtr& expr) {
        return this->walk(expr);
    }

protected:
    ExprPtr visitExpr(const ExprPtr& expr) override { return expr; }

    ExprPtr visitNonNullary(const ExprRef<NonNullaryExpr>& expr) override
    {
        ExprVector ops(expr->getNumOperands(), nullptr);

        for (size_t i = 0; i < expr->getNumOperands(); ++i) {
            ops[i] = this->getOperand(i);
        }

        return this->rewriteNonNullary(expr, ops);
    }

private:
    ExprPtr rewriteNonNullary(const ExprRef<NonNullaryExpr>& expr, const ExprVector& ops);

    ExprBuilder& mExprBuilder;
};

/// An expression rewriter that replaces certain variables with some
/// given expression, according to the values set by operator[].
class VariableExprRewrite : public ExprRewrite
{
public:
    using ExprRewrite::ExprRewrite;
    ExprPtr& operator[](Variable* variable);
    ExprPtr& operator[](const ExprRef<VarRefExpr>& expr) {
        return operator[](&expr->getVariable());
    }

protected:
    ExprPtr visitVarRef(const ExprRef<VarRefExpr>& expr) override;

private:
    llvm::DenseMap<Variable*, ExprPtr> mRewriteMap;
};

} // namespace gazer

#endif
