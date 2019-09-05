#ifndef _GAZER_CORE_EXPRTYPES_H
#define _GAZER_CORE_EXPRTYPES_H

#include "gazer/Core/Expr.h"

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
public:
    static ExprRef<NotExpr> Create(const ExprPtr& operand);

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::Not;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Expr::Not;
    }
};

// Casts
//-----------------------------------------------------------------------------

template<Expr::ExprKind Kind>
class ExtCastExpr final : public UnaryExpr
{    
    friend class ExprStorage;
    static_assert(Expr::FirstUnaryCast <= Kind && Kind <= Expr::LastUnaryCast,
        "A unary cast expression must have a unary cast expression kind.");
private:
    using UnaryExpr::UnaryExpr;

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

using ZExtExpr = ExtCastExpr<Expr::ZExt>;
using SExtExpr = ExtCastExpr<Expr::SExt>;

/// Represents an Extract expression.
/// The parameter \p offset marks the lowest order bit of the return value,
/// whereas \p offset+width-1 is the highest order bit.
/// 
/// As an example Extract(2#1111011, 0, 1) == 1, Extract(2#1111010, 0, 1) == 0.
class ExtractExpr final : public UnaryExpr
{
    // Needed for ExprStorage to call this constructor.
    friend class ExprStorage;
protected:
    template<class InputIterator>
    ExtractExpr(ExprKind kind, Type& type, InputIterator begin, InputIterator end, unsigned offset, unsigned width)
        : UnaryExpr(kind, type, begin, end), mOffset(offset), mWidth(width)
    {}

public:
    unsigned getExtractedWidth() const {
        return llvm::dyn_cast<BvType>(&getType())->getWidth();
    }

    unsigned getOffset() const { return mOffset; }
    unsigned getWidth() const { return mWidth; }

    void print(llvm::raw_ostream& os) const override;

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

/// Base class for all binary expressions.
class BinaryExpr : public NonNullaryExpr
{
protected:
    using NonNullaryExpr::NonNullaryExpr;
public:
    ExprPtr getLeft() const { return getOperand(0); }
    ExprPtr getRight() const { return getOperand(1); }
};

/// Base template for all binary arithmetic expressions.
template<Expr::ExprKind Kind>
class ArithmeticExpr final : public BinaryExpr
{
    static_assert(Expr::FirstBinaryArithmetic <= Kind && Kind <= Expr::LastBinaryArithmetic,
        "An arithmetic expression must have an arithmetic expression kind.");

    friend class ExprStorage;
protected:
    using BinaryExpr::BinaryExpr;

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

using AddExpr = ArithmeticExpr<Expr::Add>;
using SubExpr = ArithmeticExpr<Expr::Sub>;
using MulExpr = ArithmeticExpr<Expr::Mul>;
using DivExpr = ArithmeticExpr<Expr::Div>;
using BvSDivExpr = ArithmeticExpr<Expr::BvSDiv>;
using BvUDivExpr = ArithmeticExpr<Expr::BvUDiv>;
using BvSRemExpr = ArithmeticExpr<Expr::BvSRem>;
using BvURemExpr = ArithmeticExpr<Expr::BvURem>;
using ShlExpr = ArithmeticExpr<Expr::Shl>;
using LShrExpr = ArithmeticExpr<Expr::LShr>;
using AShrExpr = ArithmeticExpr<Expr::AShr>;
using BvAndExpr = ArithmeticExpr<Expr::BvAnd>;
using BvOrExpr = ArithmeticExpr<Expr::BvOr>;
using BvXorExpr = ArithmeticExpr<Expr::BvXor>;

template<Expr::ExprKind Kind>
class CompareExpr final : public BinaryExpr
{
    static_assert(Expr::FirstCompare <= Kind && Kind <= Expr::LastCompare,
        "A compare expression must have a compare expression kind.");
    friend class ExprStorage;
protected:
    using BinaryExpr::BinaryExpr;

public:
    static ExprRef<CompareExpr<Kind>> Create(const ExprPtr& left, const ExprPtr& right);
    
    static bool classof(const Expr* expr) { return expr->getKind() == Kind; }
    static bool classof(const Expr& expr) { return expr.getKind() == Kind; }
};

using EqExpr       = CompareExpr<Expr::Eq>;
using NotEqExpr    = CompareExpr<Expr::NotEq>;
using BvSLtExpr    = CompareExpr<Expr::BvSLt>;
using BvSLtEqExpr  = CompareExpr<Expr::BvSLtEq>;
using BvSGtExpr    = CompareExpr<Expr::BvSGt>;
using BvSGtEqExpr  = CompareExpr<Expr::BvSGtEq>;
using BvULtExpr    = CompareExpr<Expr::BvULt>;
using BvULtEqExpr  = CompareExpr<Expr::BvULtEq>;
using BvUGtExpr    = CompareExpr<Expr::BvUGt>;
using BvUGtEqExpr  = CompareExpr<Expr::BvUGtEq>;

template<Expr::ExprKind Kind>
class MultiaryLogicExpr final : public NonNullaryExpr
{
    static_assert(Expr::And == Kind || Expr::Or == Kind,
        "A logic expression must have a logic expression kind.");
    friend class ExprStorage;
protected:
    using NonNullaryExpr::NonNullaryExpr;

public:
    static ExprRef<MultiaryLogicExpr<Kind>> Create(const ExprPtr& left, const ExprPtr& right);
    static ExprRef<MultiaryLogicExpr<Kind>> Create(const ExprVector& ops);

    static bool classof(const Expr* expr) { return expr->getKind() == Kind; }
    static bool classof(const Expr& expr) { return expr.getKind() == Kind; }
};

using AndExpr = MultiaryLogicExpr<Expr::And>;
using OrExpr  = MultiaryLogicExpr<Expr::Or>;

template<Expr::ExprKind Kind>
class BinaryLogicExpr final : public BinaryExpr
{
    friend class ExprStorage;
protected:
    using BinaryExpr::BinaryExpr;

public:
    static ExprRef<BinaryLogicExpr<Kind>> Create(const ExprPtr& left, const ExprPtr& right);

    static bool classof(const Expr* expr) { return expr->getKind() == Kind; }
    static bool classof(const Expr& expr) { return expr.getKind() == Kind; }
};

using XorExpr = BinaryLogicExpr<Expr::Xor>;
using ImplyExpr = BinaryLogicExpr<Expr::Imply>;

// Floating-point
//-----------------------------------------------------------------------------

template<Expr::ExprKind Kind>
class FpQueryExpr final : public UnaryExpr
{
    static_assert(Kind == Expr::FIsNan || Kind == Expr::FIsInf,
        "A floating point query expression must be FIsNan or FIsInf.");
    friend class ExprStorage;
protected:
    using UnaryExpr::UnaryExpr;

public:
    static ExprRef<FpQueryExpr<Kind>> Create(const ExprPtr& operand);
};

using FIsNanExpr = FpQueryExpr<Expr::FIsNan>;
using FIsInfExpr = FpQueryExpr<Expr::FIsInf>;

namespace detail
{
    /// Helper class to deal with all floating-point expressions which store a rounding mode.
    class FpExprWithRoundingMode
    {
    public:
        FpExprWithRoundingMode(const llvm::APFloat::roundingMode& rm) : mRoundingMode(rm) {}
        llvm::APFloat::roundingMode getRoundingMode() const { return mRoundingMode; }
    protected:
        llvm::APFloat::roundingMode mRoundingMode;
    };

} // end namespace detail

template<Expr::ExprKind Kind>
class BvFpCastExpr final : public UnaryExpr, public detail::FpExprWithRoundingMode
{
    static_assert(Kind >= Expr::FCast && Kind <= Expr::FpToUnsigned, "A BvFpCastExpr must have a Bv-to-Fp or Fp-to-Bv cast kind.");
    friend class ExprStorage;
private:
    template<class InputIterator>
    BvFpCastExpr(Expr::ExprKind kind, Type& type, InputIterator begin, InputIterator end, const llvm::APFloat::roundingMode& rm)
        : UnaryExpr(kind, type, begin, end), FpExprWithRoundingMode(rm)
    {}

protected:

public:
    static ExprRef<BvFpCastExpr<Kind>> Create(const ExprPtr& operand, Type& type, const llvm::APFloat::roundingMode& rm);

    static bool classof(const Expr* expr) { return expr->getKind() == Kind; }
    static bool classof(const Expr& expr) { return expr.getKind() == Kind; }
};

using FCastExpr        = BvFpCastExpr<Expr::FCast>;
using SignedToFpExpr   = BvFpCastExpr<Expr::SignedToFp>;
using UnsignedToFpExpr = BvFpCastExpr<Expr::UnsignedToFp>;
using FpToSignedExpr   = BvFpCastExpr<Expr::FpToSigned>;
using FpToUnsignedExpr = BvFpCastExpr<Expr::FpToUnsigned>;

template<Expr::ExprKind Kind>
class FpArithmeticExpr final : public BinaryExpr, public detail::FpExprWithRoundingMode
{
    static_assert(Expr::FirstFpArithmetic <= Kind && Kind <= Expr::LastFpArithmetic,
        "An arithmetic expression must have an floating-point arithmetic expression kind.");

    // Needed for ExprStorage to call this constructor.
    friend class ExprStorage;
protected:
    template<class InputIterator>
    FpArithmeticExpr(Expr::ExprKind kind, Type& type, InputIterator begin, InputIterator end, const llvm::APFloat::roundingMode& rm)
        : BinaryExpr(kind, type, begin, end), FpExprWithRoundingMode(rm)
    {}

public:
    static ExprRef<FpArithmeticExpr<Kind>> Create(const ExprPtr& left, const ExprPtr& right, const llvm::APFloat::roundingMode& rm);

    static bool classof(const Expr* expr) { return expr->getKind() == Kind; }
    static bool classof(const Expr& expr) { return expr.getKind() == Kind; }
};


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

public:
    static ExprRef<FpCompareExpr<Kind>> Create(const ExprPtr& left, const ExprPtr& right);
    
    static bool classof(const Expr* expr) { return expr->getKind() == Kind; }
    static bool classof(const Expr& expr) { return expr.getKind() == Kind; }
};

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

public:
    static ExprRef<SelectExpr> Create(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze);

    ExprPtr getCondition() const { return getOperand(0); }
    ExprPtr getThen() const { return getOperand(1); }
    ExprPtr getElse() const { return getOperand(2); }

    static bool classof(const Expr* expr)
    { 
        return expr->getKind() == Expr::Select;
    }

    static bool classof(const Expr& expr)
    {
        return expr.getKind() == Expr::Select;
    }
};

class ArrayReadExpr final : public NonNullaryExpr
{
    friend class ExprStorage;
protected:
    using NonNullaryExpr::NonNullaryExpr;
public:
    static ExprRef<ArrayReadExpr> Create(ExprPtr array, ExprPtr index);

    ExprRef<VarRefExpr> getArrayRef() const {
        return llvm::cast<VarRefExpr>(getOperand(0));
    }

    ExprPtr getIndex() const { return getOperand(1); }

    static bool classof(const Expr* expr)
    {
        return expr->getKind() == Expr::ArrayRead;
    }

    static bool classof(const Expr& expr)
    {
        return expr.getKind() == Expr::ArrayRead;
    }
};

class ArrayWriteExpr final : public NonNullaryExpr
{
    friend class ExprStorage;
protected:
    using NonNullaryExpr::NonNullaryExpr;
public:
    static ExprRef<ArrayWriteExpr> Create(ExprPtr array, ExprPtr index, ExprPtr value);

    ExprRef<VarRefExpr> getArrayRef() const {
        return llvm::cast<VarRefExpr>(getOperand(0));
    }

    ExprPtr getIndex() const { return getOperand(1); }
    ExprPtr getElementValue() const { return getOperand(2); }

    static bool classof(const Expr* expr)
    {
        return expr->getKind() == Expr::ArrayWrite;
    }

    static bool classof(const Expr& expr)
    {
        return expr.getKind() == Expr::ArrayWrite;
    }
};

} // end namespace gazer

#endif
