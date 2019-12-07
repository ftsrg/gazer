//==-------------------------------------------------------------*- C++ -*--==//
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
#include "gazer/Core/Expr/ExprRewrite.h"

using namespace gazer;

ExprRewrite::ExprRewrite(ExprBuilder& builder)
    : mExprBuilder(builder)
{}

ExprPtr ExprRewrite::visitExpr(const ExprPtr& expr)
{
    return expr;
}

ExprPtr ExprRewrite::visitVarRef(const ExprRef<VarRefExpr>& expr)
{
    auto result = mRewriteMap[&expr->getVariable()];
    if (result != nullptr) {
        return result;
    }

    return expr;
}

ExprPtr ExprRewrite::visitNonNullary(const ExprRef<NonNullaryExpr>& expr)
{
    ExprVector ops(expr->getNumOperands(), nullptr);
    for (size_t i = 0; i < expr->getNumOperands(); ++i) {
        ops[i] = getOperand(i);
    }

    switch (expr->getKind()) {
        case Expr::Not: return mExprBuilder.Not(ops[0]);
        case Expr::ZExt: return mExprBuilder.ZExt(ops[0], llvm::cast<BvType>(expr->getType()));
        case Expr::SExt: return mExprBuilder.SExt(ops[0], llvm::cast<BvType>(expr->getType()));
        case Expr::Extract: {
            auto extract = llvm::cast<ExtractExpr>(expr);
            return mExprBuilder.Extract(ops[0], extract->getOffset(), extract->getExtractedWidth());
        }
        case Expr::Add: return mExprBuilder.Add(ops[0], ops[1]);
        case Expr::Sub: return mExprBuilder.Sub(ops[0], ops[1]);
        case Expr::Mul: return mExprBuilder.Mul(ops[0], ops[1]);
        case Expr::Div: return mExprBuilder.Div(ops[0], ops[1]);
        case Expr::Mod: return mExprBuilder.Mod(ops[0], ops[1]);
        case Expr::Rem: return mExprBuilder.Rem(ops[0], ops[1]);
        case Expr::BvSDiv: return mExprBuilder.BvSDiv(ops[0], ops[1]);
        case Expr::BvUDiv: return mExprBuilder.BvUDiv(ops[0], ops[1]);
        case Expr::BvSRem: return mExprBuilder.BvSRem(ops[0], ops[1]);
        case Expr::BvURem: return mExprBuilder.BvURem(ops[0], ops[1]);
        case Expr::Shl: return mExprBuilder.Shl(ops[0], ops[1]);
        case Expr::LShr: return mExprBuilder.LShr(ops[0], ops[1]);
        case Expr::AShr: return mExprBuilder.AShr(ops[0], ops[1]);
        case Expr::BvAnd: return mExprBuilder.BvAnd(ops[0], ops[1]);
        case Expr::BvOr: return mExprBuilder.BvOr(ops[0], ops[1]);
        case Expr::BvXor: return mExprBuilder.BvXor(ops[0], ops[1]);
        case Expr::And: return mExprBuilder.And(ops);
        case Expr::Or: return mExprBuilder.Or(ops);
        case Expr::Xor: return mExprBuilder.Xor(ops[0], ops[1]);
        case Expr::Imply: return mExprBuilder.Imply(ops[0], ops[1]);
        case Expr::Eq: return mExprBuilder.Eq(ops[0], ops[1]);
        case Expr::NotEq: return mExprBuilder.NotEq(ops[0], ops[1]);
        case Expr::Lt: return mExprBuilder.Lt(ops[0], ops[1]);
        case Expr::LtEq: return mExprBuilder.LtEq(ops[0], ops[1]);
        case Expr::Gt: return mExprBuilder.Gt(ops[0], ops[1]);
        case Expr::GtEq: return mExprBuilder.GtEq(ops[0], ops[1]);
        case Expr::BvSLt: return mExprBuilder.BvSLt(ops[0], ops[1]);
        case Expr::BvSLtEq: return mExprBuilder.BvSLtEq(ops[0], ops[1]);
        case Expr::BvSGt: return mExprBuilder.BvSGt(ops[0], ops[1]);
        case Expr::BvSGtEq: return mExprBuilder.BvSGtEq(ops[0], ops[1]);
        case Expr::BvULt: return mExprBuilder.BvULt(ops[0], ops[1]);
        case Expr::BvULtEq: return mExprBuilder.BvULtEq(ops[0], ops[1]);
        case Expr::BvUGt: return mExprBuilder.BvUGt(ops[0], ops[1]);
        case Expr::BvUGtEq: return mExprBuilder.BvUGtEq(ops[0], ops[1]);
        case Expr::FIsNan: return mExprBuilder.FIsNan(ops[0]);
        case Expr::FIsInf: return mExprBuilder.FIsInf(ops[0]);
        case Expr::FAdd: return mExprBuilder.FAdd(ops[0], ops[1], llvm::cast<FAddExpr>(expr.get())->getRoundingMode());
        case Expr::FSub: return mExprBuilder.FSub(ops[0], ops[1], llvm::cast<FSubExpr>(expr.get())->getRoundingMode());
        case Expr::FMul: return mExprBuilder.FMul(ops[0], ops[1], llvm::cast<FMulExpr>(expr.get())->getRoundingMode());
        case Expr::FDiv: return mExprBuilder.FDiv(ops[0], ops[1], llvm::cast<FDivExpr>(expr.get())->getRoundingMode());
        case Expr::FEq: return mExprBuilder.FEq(ops[0], ops[1]);
        case Expr::FGt: return mExprBuilder.FGt(ops[0], ops[1]);
        case Expr::FGtEq: return mExprBuilder.FGtEq(ops[0], ops[1]);
        case Expr::FLt: return mExprBuilder.FLt(ops[0], ops[1]);
        case Expr::FLtEq: return mExprBuilder.FLtEq(ops[0], ops[1]);
        case Expr::Select: return mExprBuilder.Select(ops[0], ops[1], ops[2]);
        case Expr::ArrayRead: return mExprBuilder.Read(ops[0], ops[1]);
        case Expr::ArrayWrite: return mExprBuilder.Write(ops[0], ops[1], ops[2]);
        default:
            llvm_unreachable("Invalid non-nullary expression kind.");
    }    
}

ExprPtr& ExprRewrite::operator[](Variable* variable)
{
    return mRewriteMap[variable];
}
