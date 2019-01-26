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
public:
    UnaryExpr(ExprKind kind, const Type& type, ExprPtr operand)
        : NonNullaryExpr(kind, type, {operand})
    {}

    using NonNullaryExpr::getOperand;
    ExprPtr getOperand() { return getOperand(0); }
};

class NotExpr final : public UnaryExpr
{
protected:
    NotExpr(ExprPtr operand)
        : UnaryExpr(Expr::Not, BoolType::get(), operand)
    {}

protected:
    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override
    {
        return new NotExpr(ops[0]);
    }

public:
    static ExprRef<NotExpr> Create(ExprPtr operand)
    {
        assert(operand->getType().isBoolType() && "Can only negate boolean expressions.");
        return ExprRef<NotExpr>(new NotExpr(operand));
    }

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
    static_assert(Expr::FirstUnaryCast <= Kind && Kind <= Expr::LastUnaryCast,
        "A unary cast expression must have a unary cast expression kind.");
private:
    ExtCastExpr(ExprPtr operand, const BvType& type)
        : UnaryExpr(Kind, type, {operand})
    {}

protected:
    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override
    {
        auto& intTy = *llvm::cast<BvType>(&getType());
        return new ExtCastExpr<Kind>(ops[0], intTy);
    }

public:
    unsigned getExtendedWidth() const {
        return llvm::dyn_cast<BvType>(&getType())->getWidth();
    }

    unsigned getWidthDiff() const {
        auto opType = llvm::dyn_cast<BvType>(&getOperand(0)->getType());
        return getExtendedWidth() - opType->getWidth();
    }

    static ExprRef<ExtCastExpr<Kind>> Create(ExprPtr operand, const Type& type) {
        assert(operand->getType().isBvType() && "Can only do bitwise cast on integers");
        assert(type.isBvType() && "Can only do bitwise cast on integers");
        
        auto lhsTy = llvm::dyn_cast<BvType>(&operand->getType());
        auto rhsTy = llvm::dyn_cast<BvType>(&type);
        assert((rhsTy->getWidth() > lhsTy->getWidth()) && "Extend casts must increase bit width");

        return ExprRef<ExtCastExpr<Kind>>(new ExtCastExpr(operand, *rhsTy));
    }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Kind;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Kind;
    }
};

using ZExtExpr = ExtCastExpr<Expr::ZExt>;
using SExtExpr = ExtCastExpr<Expr::SExt>;

class ExtractExpr final : public UnaryExpr
{
private:
    ExtractExpr(ExprPtr operand, unsigned offset, unsigned width)
        : UnaryExpr(Expr::Extract, BvType::get(width), {operand}),
            mOffset(offset), mWidth(width)
    {}

protected:
    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override
    {
        return new ExtractExpr(ops[0], mOffset, mWidth);
    }

public:
    unsigned getExtractedWidth() const {
        return llvm::dyn_cast<BvType>(&getType())->getWidth();
    }

    unsigned getOffset() const { return mOffset; }
    unsigned getWidth() const { return mWidth; }

    static ExprRef<ExtractExpr> Create(ExprPtr operand, unsigned offset, unsigned width) {
        auto opTy = llvm::dyn_cast<BvType>(&operand->getType());
        assert(opTy != nullptr && "Can only do bitwise cast on integers");
        assert(width > 0 && "Can only extract at least one bit");
        assert(opTy->getWidth() > width + offset && "Extracted bitvector must be smaller than the original");

        return ExprRef<ExtractExpr>(new ExtractExpr(operand, offset, width));
    }

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
    BinaryExpr(ExprKind kind, const Type& type, ExprPtr left, ExprPtr right)
        : NonNullaryExpr(kind, type, {left, right})
    {}
public:
    ExprPtr getLeft() const { return getOperand(0); }
    ExprPtr getRight() const { return getOperand(1); }
};

template<Expr::ExprKind Kind>
class ArithmeticExpr final : public BinaryExpr
{
    static_assert(Expr::FirstBinaryArithmetic <= Kind && Kind <= Expr::LastBinaryArithmetic,
        "An arithmetic expression must have an arithmetic expression kind.");
protected:
    ArithmeticExpr(const Type& type, ExprPtr left, ExprPtr right)
        : BinaryExpr(Kind, type, left, right)
    {}

protected:
    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return new ArithmeticExpr<Kind>(getType(), ops[0], ops[1]);
    }

public:
    static ExprRef<ArithmeticExpr<Kind>> Create(ExprPtr left, ExprPtr right)
    {
        assert((left->getType().isBvType() || left->getType().isIntType())
            && "Can only define arithmetic operations on integers.");
        assert(left->getType() == right->getType() && "Arithmetic expression operand types must match.");

        return ExprRef<ArithmeticExpr<Kind>>(new ArithmeticExpr<Kind>(left->getType(), left, right));
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
};

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
protected:
    CompareExpr(ExprPtr left, ExprPtr right)
        : BinaryExpr(Kind, BoolType::get(), left, right)
    {
        assert(left->getType() == right->getType()
            && "Compare expression operand types must match.");
    }

    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return new CompareExpr(ops[0], ops[1]);
    }

public:
    static ExprRef<CompareExpr<Kind>> Create(ExprPtr left, ExprPtr right) {
        return ExprRef<CompareExpr<Kind>>(new CompareExpr(left, right));
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
};

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
protected:
    
    template<class InputIterator>
    MultiaryLogicExpr(InputIterator begin, InputIterator end)
        : NonNullaryExpr(Kind, BoolType::get(), begin, end)
    {
        for (auto it = begin; it != end; ++it) {
            assert((*it)->getType().isBoolType() && "Logic expression operands can only be booleans.");
        }
    }

    MultiaryLogicExpr(ExprPtr left, ExprPtr right)
        : NonNullaryExpr(Kind, BoolType::get(), {left, right})
    {
        assert(left->getType().isBoolType() && "Logic expression operands can only be booleans.");
        assert((left->getType() == right->getType()) && "Logic expression operand types must match.");
    }

    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return new MultiaryLogicExpr<Kind>(ops.begin(), ops.end());
    }
public:
    static ExprRef<MultiaryLogicExpr<Kind>> Create(ExprPtr left, ExprPtr right) {
        return ExprRef<MultiaryLogicExpr<Kind>>(new MultiaryLogicExpr(left, right));
    }

    template<class InputIterator>
    static ExprRef<MultiaryLogicExpr<Kind>> Create(InputIterator begin, InputIterator end) {
        return ExprRef<MultiaryLogicExpr<Kind>>(new MultiaryLogicExpr(begin, end));
    }

    static ExprRef<MultiaryLogicExpr<Kind>> Create(const ExprVector& ops) {
        return ExprRef<MultiaryLogicExpr<Kind>>(new MultiaryLogicExpr(ops.begin(), ops.end()));
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
};

using AndExpr = MultiaryLogicExpr<Expr::And>;
using OrExpr  = MultiaryLogicExpr<Expr::Or>;

class XorExpr final : public BinaryExpr
{
protected:
    XorExpr(ExprPtr left, ExprPtr right)
        : BinaryExpr(Expr::Xor, BoolType::get(), left, right)
    {}

protected:
    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return new XorExpr(ops[0], ops[1]);
    }

public:
    static ExprRef<XorExpr> Create(ExprPtr left, ExprPtr right)
    {
        assert(left->getType().isBoolType() && "Can only XOR boolean expressions.");
        assert(right->getType().isBoolType() && "Can only XOR boolean expressions.");
        
        return ExprRef<XorExpr>(new XorExpr(left, right));
    }

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
protected:
    FpQueryExpr(ExprPtr operand)
        : UnaryExpr(Kind, BoolType::get(), operand)
    {}

    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return new FpQueryExpr<Kind>(ops[0]);   
    }
public:
    static ExprRef<FpQueryExpr<Kind>> Create(ExprPtr op)
    {
        assert(op->getType().isFloatType() && "FpQuery requrires a float operand");
        return ExprRef<FpQueryExpr<Kind>>(new FpQueryExpr<Kind>(op));
    }
};

using FIsNanExpr = FpQueryExpr<Expr::FIsNan>;
using FIsInfExpr = FpQueryExpr<Expr::FIsInf>;

template<Expr::ExprKind Kind>
class FpArithmeticExpr final : public BinaryExpr
{
    static_assert(Expr::FirstFpArithmetic <= Kind && Kind <= Expr::LastFpArithmetic,
        "An arithmetic expression must have an floating-point arithmetic expression kind.");
protected:
    FpArithmeticExpr(const FloatType& type, ExprPtr left, ExprPtr right, llvm::APFloat::roundingMode rm)
        : BinaryExpr(Kind, type, left, right), mRoundingMode(rm)
    {}

    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return new FpArithmeticExpr<Kind>(
            llvm::cast<FloatType>(getType()), ops[0], ops[1], getRoundingMode()
        );
    }

public:
    static ExprRef<FpArithmeticExpr<Kind>> Create(ExprPtr left, ExprPtr right, llvm::APFloat::roundingMode rm)
    {
        assert(left->getType().isFloatType() && "Can only define floating-point operations on float types.");
        assert(left->getType() == right->getType() && "Arithmetic expression operand types must match.");

        return ExprRef<FpArithmeticExpr<Kind>>(
            new FpArithmeticExpr<Kind>(llvm::cast<FloatType>(left->getType()), left, right, rm)
        );
    }

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

using FAddExpr = FpArithmeticExpr<Expr::FAdd>;
using FSubExpr = FpArithmeticExpr<Expr::FSub>;
using FMulExpr = FpArithmeticExpr<Expr::FMul>;
using FDivExpr = FpArithmeticExpr<Expr::FDiv>;

template<Expr::ExprKind Kind>
class FpCompareExpr final : public BinaryExpr
{
    static_assert(Expr::FirstFpCompare <= Kind && Kind <= Expr::LastFpCompare,
        "A compare expression must have a compare expression kind.");
protected:
    FpCompareExpr(ExprPtr left, ExprPtr right)
        : BinaryExpr(Kind, BoolType::get(), left, right)
    {
        assert(left->getType().isFloatType());
        assert(left->getType() == right->getType()
            && "Compare expression operand types must match.");
    }

    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return new FpCompareExpr<Kind>(ops[0], ops[1]);
    }

public:
    static ExprRef<FpCompareExpr<Kind>> Create(ExprPtr left, ExprPtr right) {
        return ExprRef<FpCompareExpr<Kind>>(new FpCompareExpr(left, right));
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
};

using FEqExpr = FpCompareExpr<Expr::FEq>;
using FGtExpr = FpCompareExpr<Expr::FGt>;
using FGtEqExpr = FpCompareExpr<Expr::FGtEq>;
using FLtExpr = FpCompareExpr<Expr::FLt>;
using FLtEqExpr = FpCompareExpr<Expr::FLtEq>;

class SelectExpr final : public NonNullaryExpr
{
protected:
    SelectExpr(const Type& type, ExprPtr condition, ExprPtr then, ExprPtr elze)
        : NonNullaryExpr(Expr::Select, type, {condition, then, elze})
    {}

    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
        return new SelectExpr(getType(), ops[0], ops[1], ops[2]);
    }
public:
    static ExprRef<SelectExpr> Create(ExprPtr condition, ExprPtr then, ExprPtr elze);

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
    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
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
    virtual ExprPtr cloneImpl(std::vector<ExprPtr> ops) const override {
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
