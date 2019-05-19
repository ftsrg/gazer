#ifndef _GAZER_CORE_EXPRTYPES_H
#define _GAZER_CORE_EXPRTYPES_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/Variable.h"

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/Casting.h>

#include <cassert>
#include <array>

namespace gazer
{

/// Base class for all unary expression kinds.
class UnaryExpr : public NonNullaryExpr
{
protected:
    using NonNullaryExpr::NonNullaryExpr;
public:
    using NonNullaryExpr::getOperand;
    ExprPtr getOperand() const { return getOperand(0); }
};

class NotExpr final : public UnaryExpr
{
    friend class ExprStorage;
protected:
    using UnaryExpr::UnaryExpr;

protected:
    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override
    {
        return Create(ops[0]);
    }

public:
    static ExprRef<NotExpr> Create(const ExprPtr& operand);

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::Not;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Expr::Not;
    }
};

//===== Casts =====//

template<Expr::ExprKind Kind>
class ExtCastExpr final : public UnaryExpr
{    
    friend class ExprStorage;
    static_assert(Expr::FirstUnaryCast <= Kind && Kind <= Expr::LastUnaryCast,
        "A unary cast expression must have a unary cast expression kind.");
private:
    using UnaryExpr::UnaryExpr;

protected:
    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return Create(ops[0], getType());
    }

public:
    unsigned getExtendedWidth() const {
        return llvm::dyn_cast<BvType>(&getType())->getWidth();
    }

    unsigned getWidthDiff() const {
        auto opType = llvm::dyn_cast<BvType>(&getOperand(0)->getType());
        return getExtendedWidth() - opType->getWidth();
    }

    static ExprRef<ExtCastExpr<Kind>> Create(const ExprPtr& operand, Type& type);

    static bool classof(const Expr* expr) {
        return expr->getKind() == Kind;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Kind;
    }
};

template class ExtCastExpr<Expr::ZExt>;
template class ExtCastExpr<Expr::SExt>;

using ZExtExpr = ExtCastExpr<Expr::ZExt>;
using SExtExpr = ExtCastExpr<Expr::SExt>;

class ExtractExpr final : public UnaryExpr
{
    // Needed for ExprStorage to call this constructor.
    friend class ExprStorage;
protected:
    template<class InputIterator>
    ExtractExpr(ExprKind kind, Type& type, InputIterator begin, InputIterator end, unsigned offset, unsigned width)
        : UnaryExpr(kind, type, begin, end), mOffset(offset), mWidth(width)
    {}

    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return Create(ops[0], mOffset, mWidth);
    }

public:
    unsigned getExtractedWidth() const {
        return llvm::dyn_cast<BvType>(&getType())->getWidth();
    }

    unsigned getOffset() const { return mOffset; }
    unsigned getWidth() const { return mWidth; }

    void print(llvm::raw_ostream& os) const;

    static ExprRef<ExtractExpr> Create(const ExprPtr& operand, unsigned offset, unsigned width);

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::Extract;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Expr::Extract;
    }
private:
    unsigned mOffset;
    unsigned mWidth;
};

class BinaryExpr : public NonNullaryExpr
{
protected:
    using NonNullaryExpr::NonNullaryExpr;
public:
    ExprPtr getLeft() const { return getOperand(0); }
    ExprPtr getRight() const { return getOperand(1); }
};

template<Expr::ExprKind Kind>
class ArithmeticExpr final : public BinaryExpr
{
    static_assert(Expr::FirstBinaryArithmetic <= Kind && Kind <= Expr::LastBinaryArithmetic,
        "An arithmetic expression must have an arithmetic expression kind.");

    friend class ExprStorage;
protected:
    using BinaryExpr::BinaryExpr;

protected:
    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return Create(ops[0], ops[1]);
    }

public:
    static ExprRef<ArithmeticExpr<Kind>> Create(const ExprPtr& left, const ExprPtr& right);

    /**
     * Type inquiry support.
     */
    static bool classof(const Expr* expr) {
        return expr->getKind() == Kind;
    }
    static bool classof(const Expr& expr) {
        return expr.getKind() == Kind;
    }
};

template class ArithmeticExpr<Expr::Add>;
template class ArithmeticExpr<Expr::Sub>;
template class ArithmeticExpr<Expr::Mul>;
template class ArithmeticExpr<Expr::SDiv>;
template class ArithmeticExpr<Expr::UDiv>;
template class ArithmeticExpr<Expr::SRem>;
template class ArithmeticExpr<Expr::URem>;
template class ArithmeticExpr<Expr::Shl>;
template class ArithmeticExpr<Expr::LShr>;
template class ArithmeticExpr<Expr::AShr>;
template class ArithmeticExpr<Expr::BAnd>;
template class ArithmeticExpr<Expr::BOr>;
template class ArithmeticExpr<Expr::BXor>;

using AddExpr = ArithmeticExpr<Expr::Add>;
using SubExpr = ArithmeticExpr<Expr::Sub>;
using MulExpr = ArithmeticExpr<Expr::Mul>;
using SDivExpr = ArithmeticExpr<Expr::SDiv>;
using UDivExpr = ArithmeticExpr<Expr::UDiv>;
using SRemExpr = ArithmeticExpr<Expr::SRem>;
using URemExpr = ArithmeticExpr<Expr::URem>;
using ShlExpr = ArithmeticExpr<Expr::Shl>;
using LShrExpr = ArithmeticExpr<Expr::LShr>;
using AShrExpr = ArithmeticExpr<Expr::AShr>;
using BAndExpr = ArithmeticExpr<Expr::BAnd>;
using BOrExpr = ArithmeticExpr<Expr::BOr>;
using BXorExpr = ArithmeticExpr<Expr::BXor>;

template<Expr::ExprKind Kind>
class CompareExpr final : public BinaryExpr
{
    static_assert(Expr::FirstCompare <= Kind && Kind <= Expr::LastCompare,
        "A compare expression must have a compare expression kind.");
    friend class ExprStorage;
protected:
    using BinaryExpr::BinaryExpr;

    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return Create(ops[0], ops[1]);
    }

public:
    static ExprRef<CompareExpr<Kind>> Create(ExprPtr left, ExprPtr right);
    
    /**
     * Type inquiry support.
     */
    static bool classof(const Expr* expr) {
        return expr->getKind() == Kind;
    }
    static bool classof(const Expr& expr) {
        return expr.getKind() == Kind;
    }
};

template class CompareExpr<Expr::Eq>;
template class CompareExpr<Expr::NotEq>;
template class CompareExpr<Expr::SLt>;
template class CompareExpr<Expr::SLtEq>;
template class CompareExpr<Expr::SGt>;
template class CompareExpr<Expr::SGtEq>;
template class CompareExpr<Expr::ULt>;
template class CompareExpr<Expr::ULtEq>;
template class CompareExpr<Expr::UGt>;
template class CompareExpr<Expr::UGtEq>;

using EqExpr     = CompareExpr<Expr::Eq>;
using NotEqExpr  = CompareExpr<Expr::NotEq>;
using SLtExpr    = CompareExpr<Expr::SLt>;
using SLtEqExpr  = CompareExpr<Expr::SLtEq>;
using SGtExpr    = CompareExpr<Expr::SGt>;
using SGtEqExpr  = CompareExpr<Expr::SGtEq>;
using ULtExpr    = CompareExpr<Expr::ULt>;
using ULtEqExpr  = CompareExpr<Expr::ULtEq>;
using UGtExpr    = CompareExpr<Expr::UGt>;
using UGtEqExpr  = CompareExpr<Expr::UGtEq>;

template<Expr::ExprKind Kind>
class MultiaryLogicExpr final : public NonNullaryExpr
{
    static_assert(Expr::And == Kind || Expr::Or == Kind,
        "A logic expression must have a logic expression kind.");
    friend class ExprStorage;
protected:
    using NonNullaryExpr::NonNullaryExpr;

    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return Create(ops);
    }
public:
    static ExprRef<MultiaryLogicExpr<Kind>> Create(const ExprPtr& left, const ExprPtr& right) {
        return Create({left, right});
    }
    static ExprRef<MultiaryLogicExpr<Kind>> Create(std::initializer_list<ExprPtr> ops);
    static ExprRef<MultiaryLogicExpr<Kind>> Create(const ExprVector& ops);
    
    /**
     * Type inquiry support.
     */
    static bool classof(const Expr* expr) {
        return expr->getKind() == Kind;
    }
    static bool classof(const Expr& expr) {
        return expr.getKind() == Kind;
    }
};

template class MultiaryLogicExpr<Expr::And>;
template class MultiaryLogicExpr<Expr::Or>;

using AndExpr = MultiaryLogicExpr<Expr::And>;
using OrExpr  = MultiaryLogicExpr<Expr::Or>;

class XorExpr final : public BinaryExpr
{
    friend class ExprStorage;
protected:
    using BinaryExpr::BinaryExpr;

protected:
    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return Create(ops[0], ops[1]);
    }

public:
    static ExprRef<XorExpr> Create(const ExprPtr& left, const ExprPtr& right);

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::Xor;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Expr::Xor;
    }
};

template<Expr::ExprKind Kind>
class FpQueryExpr final : public UnaryExpr
{
    static_assert(Kind == Expr::FIsNan || Kind == Expr::FIsInf,
        "A floating point query expression must be FIsNan or FIsInf");
    friend class ExprStorage;
protected:
    using UnaryExpr::UnaryExpr;

    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return Create(ops[0]);
    }
public:
    static ExprRef<FpQueryExpr<Kind>> Create(const ExprPtr& operand);
};

template class FpQueryExpr<Expr::FIsNan>;
template class FpQueryExpr<Expr::FIsInf>;

using FIsNanExpr = FpQueryExpr<Expr::FIsNan>;
using FIsInfExpr = FpQueryExpr<Expr::FIsInf>;

template<Expr::ExprKind Kind>
class FpArithmeticExpr final : public BinaryExpr
{
    static_assert(Expr::FirstFpArithmetic <= Kind && Kind <= Expr::LastFpArithmetic,
        "An arithmetic expression must have an floating-point arithmetic expression kind.");

    // Needed for ExprStorage to call this constructor.
    friend class ExprStorage;
protected:
    template<class InputIterator>
    FpArithmeticExpr(Expr::ExprKind kind, Type& type, InputIterator begin, InputIterator end, const llvm::APFloat::roundingMode& rm)
        : BinaryExpr(kind, type, begin, end), mRoundingMode(rm)
    {}

    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return Create(ops[0], ops[1], mRoundingMode);
    }
public:
    static ExprRef<FpArithmeticExpr<Kind>> Create(const ExprPtr& left, const ExprPtr& right, const llvm::APFloat::roundingMode& rm);

    llvm::APFloat::roundingMode getRoundingMode() const {
        return mRoundingMode;
    }

    /**
     * Type inquiry support.
     */
    static bool classof(const Expr* expr) {
        return expr->getKind() == Kind;
    }
    static bool classof(const Expr& expr) {
        return expr.getKind() == Kind;
    }
private:
    llvm::APFloat::roundingMode mRoundingMode;
};

template class FpArithmeticExpr<Expr::FAdd>;
template class FpArithmeticExpr<Expr::FSub>;
template class FpArithmeticExpr<Expr::FMul>;
template class FpArithmeticExpr<Expr::FDiv>;

using FAddExpr = FpArithmeticExpr<Expr::FAdd>;
using FSubExpr = FpArithmeticExpr<Expr::FSub>;
using FMulExpr = FpArithmeticExpr<Expr::FMul>;
using FDivExpr = FpArithmeticExpr<Expr::FDiv>;

template<Expr::ExprKind Kind>
class FpCompareExpr final : public BinaryExpr
{
    static_assert(Expr::FirstFpCompare <= Kind && Kind <= Expr::LastFpCompare,
        "A compare expression must have a compare expression kind.");
    friend class ExprStorage;
protected:
    using BinaryExpr::BinaryExpr;

    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return Create(ops[0], ops[1]);
    }

public:
    static ExprRef<FpCompareExpr<Kind>> Create(const ExprPtr& left, const ExprPtr& right);
    
    /**
     * Type inquiry support.
     */
    static bool classof(const Expr* expr) {
        return expr->getKind() == Kind;
    }
    static bool classof(const Expr& expr) {
        return expr.getKind() == Kind;
    }
};

template class FpCompareExpr<Expr::FEq>;
template class FpCompareExpr<Expr::FGt>;
template class FpCompareExpr<Expr::FGtEq>;
template class FpCompareExpr<Expr::FLt>;
template class FpCompareExpr<Expr::FLtEq>;

using FEqExpr = FpCompareExpr<Expr::FEq>;
using FGtExpr = FpCompareExpr<Expr::FGt>;
using FGtEqExpr = FpCompareExpr<Expr::FGtEq>;
using FLtExpr = FpCompareExpr<Expr::FLt>;
using FLtEqExpr = FpCompareExpr<Expr::FLtEq>;

class SelectExpr final : public NonNullaryExpr
{
    friend class ExprStorage;
protected:
    using NonNullaryExpr::NonNullaryExpr;

    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return Create(ops[0], ops[1], ops[2]);
    }
public:
    static ExprRef<SelectExpr> Create(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze);

    ExprPtr getCondition() const { return getOperand(0); }
    ExprPtr getThen() const { return getOperand(1); }
    ExprPtr getElse() const { return getOperand(2); }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::Select;
    }
    static bool classof(const Expr& expr) {
        return expr.getKind() == Expr::Select;
    }
};


class ArrayReadExpr final : public NonNullaryExpr
{
protected:
    ArrayReadExpr(ExprPtr array, ExprPtr index)
        : NonNullaryExpr(Expr::ArrayRead, array->getType(), {array, index})
    {}
public:
    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return new ArrayReadExpr(ops[0], ops[1]);
    }

    ExprRef<VarRefExpr> getArrayRef() const {
        return llvm::cast<VarRefExpr>(getOperand(0));
    }

    ExprPtr getIndex() const { return getOperand(1); }

    static ExprRef<ArrayReadExpr> Create(ExprRef<VarRefExpr> array, ExprPtr index);

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::ArrayRead;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Expr::ArrayRead;
    }
};

class ArrayWriteExpr final : public NonNullaryExpr
{
protected:
    ArrayWriteExpr(ExprPtr array, ExprPtr index, ExprPtr value)
        : NonNullaryExpr(Expr::ArrayRead, array->getType(), {array, index, value})
    {}
public:
    ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return new ArrayWriteExpr(ops[0], ops[1], ops[2]);
    }

    ExprRef<VarRefExpr> getArrayRef() const {
        return llvm::cast<VarRefExpr>(getOperand(0));
    }
    ExprPtr getIndex() const { return getOperand(1); }
    ExprPtr getElementValue() const { return getOperand(2); }

    static ExprRef<ArrayWriteExpr> Create(
        ExprRef<VarRefExpr> array, ExprPtr index, ExprPtr value
    );

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::ArrayWrite;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Expr::ArrayWrite;
    }
};

} // end namespace gazer

#endif
