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
#include "gazer/Core/Expr/ExprEvaluator.h"

#include <llvm/Support/Debug.h>
#include <llvm/Support/raw_ostream.h>

#define DEBUG_TYPE "ExprEvaluator"

using namespace gazer;
using llvm::cast;
using llvm::dyn_cast;

auto ValuationExprEvaluator::getVariableValue(Variable& variable)
    -> ExprRef<AtomicExpr>
{
    auto result = mValuation.find(&variable);
    if (result == mValuation.end()) {
        return UndefExpr::Get(variable.getType());
    }

    return result->second;
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitUndef(const ExprRef<UndefExpr>& expr)
{
    return expr;
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitExpr(const ExprPtr& expr)
{
    LLVM_DEBUG(llvm::dbgs() << "Unhandled expression: " << *expr << "\n");
    llvm_unreachable("Unhandled expression type in ExprEvaluatorBase");
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitLiteral(const ExprRef<LiteralExpr>& expr) {
    return expr;
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitVarRef(const ExprRef<VarRefExpr>& expr) {
    return this->getVariableValue(expr->getVariable());
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitZExt(const ExprRef<ZExtExpr>& expr)
{
    auto bvLit = dyn_cast<BvLiteralExpr>(getOperand(0));
    auto& type = llvm::cast<BvType>(expr->getType());

    return BvLiteralExpr::Get(type, bvLit->getValue().zext(expr->getExtendedWidth()));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitSExt(const ExprRef<SExtExpr>& expr)
{
    auto bvLit = dyn_cast<BvLiteralExpr>(getOperand(0));
    auto& type = llvm::cast<BvType>(expr->getType());

    return BvLiteralExpr::Get(type, bvLit->getValue().sext(expr->getExtendedWidth()));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitBvConcat(const ExprRef<BvConcatExpr>& expr)
{
    auto left = getOperand(0);
    auto right = getOperand(1);

    if (left->isUndef() || right->isUndef()) {
        return UndefExpr::Get(expr->getType());
    }

    auto leftBv = llvm::cast<BvLiteralExpr>(left);
    auto rightBv = llvm::cast<BvLiteralExpr>(right);

    unsigned size = leftBv->getType().getWidth() + rightBv->getType().getWidth();
    llvm::APInt result = leftBv->getValue();

    // Zero-extend to the new size, shift to the left and or the bits together
    result = result.zext(size).shl(rightBv->getType().getWidth()) | rightBv->getValue().zext(size);

    return BvLiteralExpr::Get(BvType::Get(expr->getContext(), size), result);
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitExtract(const ExprRef<ExtractExpr>& expr)
{
    auto bvLit = dyn_cast<BvLiteralExpr>(getOperand(0));

    return BvLiteralExpr::Get(
        cast<BvType>(expr->getType()),
        bvLit->getValue().extractBits(expr->getExtractedWidth(), expr->getOffset())
    );
}

static ExprRef<AtomicExpr> EvalBinaryArithmetic(
    Expr::ExprKind kind,
    const ExprRef<AtomicExpr>& lhs,
    const ExprRef<AtomicExpr>& rhs)
{
    assert(lhs->getType() == rhs->getType());

    // Handle the undef cases first.
    // FIXME: We should use some arithmetic rules to eliminate undef's if it is
    //   possible, e.g. multiplying an undef with zero.
    if (lhs->isUndef() || rhs->isUndef()) {
        return UndefExpr::Get(lhs->getType());
    }

    if (lhs->getType().isBvType()) {
        auto leftBv  = llvm::cast<BvLiteralExpr>(lhs)->getValue();
        auto rightBv = llvm::cast<BvLiteralExpr>(rhs)->getValue();

        auto& type = llvm::cast<BvType>(lhs->getType());

        switch (kind) {
            case Expr::Add: return BvLiteralExpr::Get(type, leftBv + rightBv);
            case Expr::Sub: return BvLiteralExpr::Get(type, leftBv - rightBv);
            case Expr::Mul: return BvLiteralExpr::Get(type, leftBv * rightBv);
            case Expr::BvSDiv: return BvLiteralExpr::Get(type, leftBv.sdiv(rightBv));
            case Expr::BvUDiv: return BvLiteralExpr::Get(type, leftBv.udiv(rightBv));
            case Expr::BvSRem: return BvLiteralExpr::Get(type, leftBv.srem(rightBv));
            case Expr::BvURem: return BvLiteralExpr::Get(type, leftBv.urem(rightBv));
            case Expr::Shl: return BvLiteralExpr::Get(type, leftBv.shl(rightBv));
            case Expr::LShr: return BvLiteralExpr::Get(type, leftBv.lshr(rightBv.getLimitedValue()));
            case Expr::AShr: return BvLiteralExpr::Get(type, leftBv.ashr(rightBv.getLimitedValue()));
            case Expr::BvAnd: return BvLiteralExpr::Get(type, leftBv & rightBv);
            case Expr::BvOr: return BvLiteralExpr::Get(type, leftBv | rightBv);
            case Expr::BvXor: return BvLiteralExpr::Get(type, leftBv ^ rightBv);
            default:
                llvm_unreachable("Unknown binary expression kind!");
        }
    }
    
    if (lhs->getType().isIntType()) {
        auto left  = llvm::cast<IntLiteralExpr>(lhs)->getValue();
        auto right = llvm::cast<IntLiteralExpr>(rhs)->getValue();

        auto& intTy = llvm::cast<IntType>(lhs->getType());

        switch (kind) {
            case Expr::Add: return IntLiteralExpr::Get(intTy, left + right);
            case Expr::Sub: return IntLiteralExpr::Get(intTy, left - right);
            case Expr::Mul: return IntLiteralExpr::Get(intTy, left * right);
            case Expr::Div: return IntLiteralExpr::Get(intTy, left / right);
            case Expr::Mod: {
                auto result = left % right;
                if (result < 0) {
                    result += std::abs(right);
                }
                return IntLiteralExpr::Get(intTy, result);
            }
            case Expr::Rem:
                // Division remainder, for example:
                //  5 rem  3 = 2
                //  5 rem -3 = 2
                // -5 rem  3 = -2
                // -5 rem -3 = -2
                return IntLiteralExpr::Get(intTy, left % right);
            default:
                break;
        }
    }
    
    llvm_unreachable("Unknown binary arithmetic expression kind.");
}

// Binary
ExprRef<AtomicExpr> ExprEvaluatorBase::visitAdd(const ExprRef<AddExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitSub(const ExprRef<SubExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitMul(const ExprRef<MulExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitDiv(const ExprRef<DivExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitMod(const ExprRef<ModExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitRem(const ExprRef<RemExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitBvSDiv(const ExprRef<BvSDivExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitBvUDiv(const ExprRef<BvUDivExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitBvSRem(const ExprRef<BvSRemExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitBvURem(const ExprRef<BvURemExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitShl(const ExprRef<ShlExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitLShr(const ExprRef<LShrExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitAShr(const ExprRef<AShrExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitBvAnd(const ExprRef<BvAndExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitBvOr(const ExprRef<BvOrExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitBvXor(const ExprRef<BvXorExpr>& expr)
{
    return EvalBinaryArithmetic(expr->getKind(), getOperand(0), getOperand(1));
}

// Logic
//-----------------------------------------------------------------------------

ExprRef<AtomicExpr> ExprEvaluatorBase::visitNot(const ExprRef<NotExpr>& expr)
{
    auto boolLit = cast<BoolLiteralExpr>(getOperand(0).get());
    return BoolLiteralExpr::Get(cast<BoolType>(expr->getType()), !boolLit->getValue());
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitAnd(const ExprRef<AndExpr>& expr)
{
    bool isUndef = false;

    for (size_t i = 0; i < expr->getNumOperands(); ++i) {
        auto operand = getOperand(i);
        if (operand->isUndef()) {
            isUndef = true;
        }
        
        bool value = cast<BoolLiteralExpr>(operand)->getValue();
        if (!value) {
            return BoolLiteralExpr::False(expr->getContext());
        }
    }

    if (isUndef) {
        return UndefExpr::Get(expr->getType());
    }

    return BoolLiteralExpr::True(BoolType::Get(expr->getContext()));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitOr(const ExprRef<OrExpr>& expr)
{
    bool isUndef = false;

    for (size_t i = 0; i < expr->getNumOperands(); ++i) {
        auto operand = getOperand(i);
        if (operand->isUndef()) {
            isUndef = true;
        }
        
        bool value = cast<BoolLiteralExpr>(operand)->getValue();
        if (value) {
            return BoolLiteralExpr::True(expr->getContext());
        }
    }

    if (isUndef) {
        return UndefExpr::Get(expr->getType());
    }

    return BoolLiteralExpr::False(BoolType::Get(expr->getContext()));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitImply(const ExprRef<ImplyExpr>& expr)
{
    auto left  = getOperand(0);
    auto right = getOperand(1);

    ExprRef<BoolLiteralExpr> leftBl, rightBl;

    if (left->isUndef()) {
        // If the right side is true, then the whole expression is true.
        if ((rightBl = dyn_cast<BoolLiteralExpr>(right)) && rightBl->isTrue()) {
            return BoolLiteralExpr::True(expr->getContext());
        }

        return left;
    }

    if (right->isUndef()) {
        // If the right side is false, then the whole expression is true.
        if ((leftBl = dyn_cast<BoolLiteralExpr>(left)) && leftBl->isFalse()) {
            return BoolLiteralExpr::True(expr->getContext());
        }

        return right;
    }

    leftBl  = cast<BoolLiteralExpr>(getOperand(0));
    rightBl = cast<BoolLiteralExpr>(getOperand(1));

    return BoolLiteralExpr::Get(cast<BoolType>(expr->getType()), !leftBl->getValue() || rightBl->getValue());
}

static ExprRef<AtomicExpr> EvalBvCompare(
    Expr::ExprKind kind,
    const ExprRef<AtomicExpr>& lhs,
    const ExprRef<AtomicExpr>& rhs)
{
    assert(lhs->getType().isBvType());

    auto left = cast<BvLiteralExpr>(lhs)->getValue();
    auto right = cast<BvLiteralExpr>(rhs)->getValue();

    BoolType& type = BoolType::Get(lhs->getContext());

    switch (kind) {
        case Expr::Eq: return BoolLiteralExpr::Get(type, left.eq(right));
        case Expr::NotEq: return BoolLiteralExpr::Get(type, left.ne(right));
        case Expr::BvSLt: return BoolLiteralExpr::Get(type, left.slt(right));
        case Expr::BvSLtEq: return BoolLiteralExpr::Get(type, left.sle(right));
        case Expr::BvSGt: return BoolLiteralExpr::Get(type, left.sgt(right));
        case Expr::BvSGtEq: return BoolLiteralExpr::Get(type, left.sge(right));
        case Expr::BvULt: return BoolLiteralExpr::Get(type, left.ult(right));
        case Expr::BvULtEq: return BoolLiteralExpr::Get(type, left.ule(right));
        case Expr::BvUGt: return BoolLiteralExpr::Get(type, left.ugt(right));
        case Expr::BvUGtEq: return BoolLiteralExpr::Get(type, left.uge(right));
        default:
            break;
    }

    llvm_unreachable("Unknown bit-vector comparison kind.");
}

static ExprRef<AtomicExpr> EvalIntCompare(
    Expr::ExprKind kind,
    const ExprRef<AtomicExpr>& lhs,
    const ExprRef<AtomicExpr>& rhs)
{
    assert(lhs->getType().isIntType());
    assert(rhs->getType().isIntType());

    auto left = llvm::cast<IntLiteralExpr>(lhs)->getValue();
    auto right = llvm::cast<IntLiteralExpr>(rhs)->getValue();

    BoolType& type = BoolType::Get(lhs->getContext());

    switch (kind) {
        case Expr::Eq: return BoolLiteralExpr::Get(type, left == right);
        case Expr::NotEq: return BoolLiteralExpr::Get(type, left != right);
        case Expr::Lt: return BoolLiteralExpr::Get(type, left < right);
        case Expr::LtEq: return BoolLiteralExpr::Get(type, left <= right);
        case Expr::Gt: return BoolLiteralExpr::Get(type, left > right);
        case Expr::GtEq: return BoolLiteralExpr::Get(type, left >= right);
        default:
            break;
    }

    llvm_unreachable("Unknown integer comparison kind.");
}

static ExprRef<AtomicExpr> EvalBinaryCompare(
    Expr::ExprKind kind, const ExprRef<AtomicExpr>& left, const ExprRef<AtomicExpr>& right)
{
    Type& opTy = left->getType();
    assert(left->getType() == right->getType());
    assert(!opTy.isFloatType() && "Float types must be compared using FEqExpr!");

    if (left->isUndef() || right->isUndef()) {
        return UndefExpr::Get(BoolType::Get(left->getContext()));
    }

    switch (opTy.getTypeID()) {
        case Type::BvTypeID:
            return EvalBvCompare(kind, left, right);
        case Type::BoolTypeID: {
            BoolType& boolTy = BoolType::Get(opTy.getContext());
            assert(kind == Expr::Eq || kind == Expr::NotEq);

            return BoolLiteralExpr::Get(
                boolTy,
                cast<BoolLiteralExpr>(left)->getValue() == cast<BoolLiteralExpr>(right)->getValue()
            );
        }
        case Type::IntTypeID:
            return EvalIntCompare(kind, left, right);
        default:
            break;
    }

    llvm_unreachable("Invalid operand type in a comparison expression!");
}

#define HANDLE_BINARY_COMPARE(KIND)                                                         \
    ExprRef<AtomicExpr> ExprEvaluatorBase::visit##KIND(const ExprRef<KIND##Expr>& expr) {   \
        return EvalBinaryCompare(Expr::KIND, getOperand(0), getOperand(1));                 \
    }                                                                                       \

HANDLE_BINARY_COMPARE(Eq)
HANDLE_BINARY_COMPARE(NotEq)
HANDLE_BINARY_COMPARE(Lt)
HANDLE_BINARY_COMPARE(LtEq)
HANDLE_BINARY_COMPARE(Gt)
HANDLE_BINARY_COMPARE(GtEq)
HANDLE_BINARY_COMPARE(BvSLt)
HANDLE_BINARY_COMPARE(BvSLtEq)
HANDLE_BINARY_COMPARE(BvSGt)
HANDLE_BINARY_COMPARE(BvSGtEq)
HANDLE_BINARY_COMPARE(BvULt)
HANDLE_BINARY_COMPARE(BvULtEq)
HANDLE_BINARY_COMPARE(BvUGt)
HANDLE_BINARY_COMPARE(BvUGtEq)

#undef HANDLE_BINARY_COMPARE

// Floating-point queries
ExprRef<AtomicExpr> ExprEvaluatorBase::visitFIsNan(const ExprRef<FIsNanExpr>& expr)
{
    auto fpLit = llvm::cast<FloatLiteralExpr>(getOperand(0));
    return BoolLiteralExpr::Get(BoolType::Get(expr->getContext()), fpLit->getValue().isNaN());
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitFIsInf(const ExprRef<FIsInfExpr>& expr)
{
    auto fpLit = llvm::cast<FloatLiteralExpr>(getOperand(0));
    return BoolLiteralExpr::Get(BoolType::Get(expr->getContext()), fpLit->getValue().isInfinity());
}

// Floating-point arithmetic
ExprRef<AtomicExpr> ExprEvaluatorBase::visitFAdd(const ExprRef<FAddExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<AtomicExpr> ExprEvaluatorBase::visitFSub(const ExprRef<FSubExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<AtomicExpr> ExprEvaluatorBase::visitFMul(const ExprRef<FMulExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<AtomicExpr> ExprEvaluatorBase::visitFDiv(const ExprRef<FDivExpr>& expr) {
    return this->visitNonNullary(expr);
}

// Floating-point compare
ExprRef<AtomicExpr> ExprEvaluatorBase::visitFEq(const ExprRef<FEqExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<AtomicExpr> ExprEvaluatorBase::visitFGt(const ExprRef<FGtExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<AtomicExpr> ExprEvaluatorBase::visitFGtEq(const ExprRef<FGtEqExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<AtomicExpr> ExprEvaluatorBase::visitFLt(const ExprRef<FLtExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<AtomicExpr> ExprEvaluatorBase::visitFLtEq(const ExprRef<FLtEqExpr>& expr) {
    return this->visitNonNullary(expr);
}

// Floating-point casts
ExprRef<AtomicExpr> ExprEvaluatorBase::visitFCast(const ExprRef<FCastExpr>& expr)
{
    return this->visitNonNullary(expr);
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitSignedToFp(const ExprRef<SignedToFpExpr>& expr)
{
    return this->visitNonNullary(expr);
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitUnsignedToFp(const ExprRef<UnsignedToFpExpr>& expr)
{
    return this->visitNonNullary(expr);
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitFpToSigned(const ExprRef<FpToSignedExpr>& expr)
{
    return this->visitNonNullary(expr);
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitFpToUnsigned(const ExprRef<FpToUnsignedExpr>& expr)
{
    return this->visitNonNullary(expr);
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitFpToBv(const ExprRef<FpToBvExpr>& expr)
{
    auto fpLit = llvm::cast<FloatLiteralExpr>(getOperand(0));
    auto& bvType = llvm::cast<BvType>(expr->getType());

    return BvLiteralExpr::Get(bvType, fpLit->getValue().bitcastToAPInt());
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitBvToFp(const ExprRef<BvToFpExpr>& expr)
{
    auto bvLit = llvm::cast<BvLiteralExpr>(getOperand(0));
    auto& fltTy = llvm::cast<FloatType>(expr->getType());

    llvm::APFloat apFloat(fltTy.getLLVMSemantics(), bvLit->getValue());
    return FloatLiteralExpr::Get(fltTy, apFloat);
}

template<class Type, class ExprTy>
ExprRef<AtomicExpr> EvalSelect(
    const ExprRef<BoolLiteralExpr>& cond,
    const ExprRef<AtomicExpr>& then,
    const ExprRef<AtomicExpr>& elze
) {
    return ExprTy::Get(
        *cast<Type>(&then->getType()),
        cond->getValue() ? cast<ExprTy>(then)->getValue() : cast<ExprTy>(elze)->getValue()
    );
}

// Ternary
ExprRef<AtomicExpr> ExprEvaluatorBase::visitSelect(const ExprRef<SelectExpr>& expr)
{
    auto cond = cast<BoolLiteralExpr>(getOperand(0));
    auto then = getOperand(1);
    auto elze = getOperand(2);

    switch (expr->getType().getTypeID()) {
        case Type::BoolTypeID:
            return EvalSelect<BoolType, BoolLiteralExpr>(cond, then, elze);
        case Type::BvTypeID:
            return EvalSelect<BvType, BvLiteralExpr>(cond, then, elze);
        case Type::IntTypeID:
            return EvalSelect<IntType, IntLiteralExpr>(cond, then, elze);
        case Type::FloatTypeID:
            return EvalSelect<FloatType, FloatLiteralExpr>(cond, then, elze);
        case Type::RealTypeID:
            return EvalSelect<RealType, RealLiteralExpr>(cond, then, elze);
    }

    llvm_unreachable("Invalid SelectExpr type!");
}

// Arrays
ExprRef<AtomicExpr> ExprEvaluatorBase::visitArrayRead(const ExprRef<ArrayReadExpr>& expr)
{
    auto array = getOperand(0);
    if (array->isUndef()) {
        return UndefExpr::Get(expr->getType());
    }

    auto litArray = expr_cast<ArrayLiteralExpr>(array);

    auto index = getOperand(1);
    if (index->isUndef()) {
        return litArray->getDefault();
    }

    return litArray->getValue(llvm::cast<LiteralExpr>(index));
}

ExprRef<AtomicExpr> ExprEvaluatorBase::visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr)
{
    return this->visitNonNullary(expr);
}