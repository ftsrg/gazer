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

class ExprRewrite : public ExprWalker<ExprRewrite, ExprPtr>
{
public:
    explicit ExprRewrite(ExprBuilder& builder);
    ExprPtr& operator[](Variable* variable);

public:
    ExprPtr visitExpr(const ExprPtr& expr);
    ExprPtr visitVarRef(const ExprRef<VarRefExpr>& expr);
    ExprPtr visitNonNullary(const ExprRef<NonNullaryExpr>& expr);

private:
    llvm::DenseMap<Variable*, ExprPtr> mRewriteMap;
    ExprBuilder& mExprBuilder;
};

}

#endif
