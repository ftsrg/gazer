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

/// Non-template dependent functionality of the expression rewriter.
class ExprRewriteBase
{
protected:
    ExprRewriteBase(ExprBuilder& builder)
        : mExprBuilder(builder)
    {}

    ExprPtr rewriteNonNullary(const ExprRef<NonNullaryExpr>& expr, const ExprVector& ops);
protected:
    ExprBuilder& mExprBuilder;
};

/// Base class for expression rewrite implementations.
template<class DerivedT>
class ExprRewrite : public ExprWalker<DerivedT, ExprPtr>, public ExprRewriteBase
{
    friend class ExprWalker<DerivedT, ExprPtr>;
public:
    explicit ExprRewrite(ExprBuilder& builder)
        : ExprRewriteBase(builder)
    {}

protected:
    ExprPtr visitExpr(const ExprPtr& expr) { return expr; }

    ExprPtr visitNonNullary(const ExprRef<NonNullaryExpr>& expr)
    {
        ExprVector ops(expr->getNumOperands(), nullptr);
        for (size_t i = 0; i < expr->getNumOperands(); ++i) {
            ops[i] = this->getOperand(i);
        }

        return this->rewriteNonNullary(expr, ops);
    }
};

/// An expression rewriter that replaces certain variables with some
/// given expression, according to the values set by operator[].
class VariableExprRewrite : public ExprRewrite<VariableExprRewrite>
{
    friend class ExprWalker<VariableExprRewrite, ExprPtr>;
public:
    explicit VariableExprRewrite(ExprBuilder& builder)
        : ExprRewrite(builder)
    {}

    ExprPtr& operator[](Variable* variable);

protected:
    ExprPtr visitVarRef(const ExprRef<VarRefExpr>& expr);

private:
    llvm::DenseMap<Variable*, ExprPtr> mRewriteMap;
};

}

#endif
