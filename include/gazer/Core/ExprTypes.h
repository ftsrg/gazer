#ifndef _GAZER_CORE_EXPRTYPES_H
#define _GAZER_CORE_EXPRTYPES_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/Variable.h"

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/Casting.h>

#include <cassert>
#include <array>

namespace gazer
{

//============
// Unary expressions
//============

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
        : UnaryExpr(Expr::Not, *BoolType::get(), operand)
    {}

protected:
    virtual Expr* withOps(std::vector<ExprPtr> ops) const override {
        return new NotExpr(ops[0]);
    }

public:
    static std::shared_ptr<NotExpr> Create(ExprPtr operand)
    {
        assert(operand->getType().isBoolType() && "Can only negate boolean expressions.");
        return std::shared_ptr<NotExpr>(new NotExpr(operand));
    }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::Not;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Expr::Not;
    }
};

template<Expr::ExprKind Kind>
class ExtCastExpr final : public UnaryExpr
{    
    static_assert(Expr::FirstUnaryCast <= Kind && Kind <= Expr::LastUnaryCast,
        "A unary cast expression must have a unary cast expression kind.");
private:
    ExtCastExpr(ExprPtr operand, const IntType& type)
        : UnaryExpr(Kind, type, {operand})
    {}

protected:
    virtual Expr* withOps(std::vector<ExprPtr> ops) const override {
        auto& intTy = *llvm::dyn_cast<IntType>(&getType());
        return new ExtCastExpr<Kind>(ops[0], intTy);
    }

public:
    unsigned getExtendedWidth() const {
        return llvm::dyn_cast<IntType>(&getType())->getWidth();
    }
    unsigned getWidthDiff() const {
        auto opType = llvm::dyn_cast<IntType>(&getOperand(0)->getType());
        return getExtendedWidth() - opType->getWidth();
    }

    static std::shared_ptr<ExtCastExpr<Kind>> Create(ExprPtr operand, const Type& type) {
        assert(operand->getType().isIntType() && "Can only do bitwise cast on integers");
        assert(type.isIntType() && "Can only do bitwise cast on integers");
        
        auto lhsTy = llvm::dyn_cast<IntType>(&operand->getType());
        auto rhsTy = llvm::dyn_cast<IntType>(&type);
        if (lhsTy->getWidth() >= rhsTy->getWidth()) {
            throw TypeCastError("Extend casts must increase bit width");
        }

        return std::shared_ptr<ExtCastExpr<Kind>>(new ExtCastExpr(operand, *rhsTy));
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
        : UnaryExpr(Expr::Extract, *IntType::get(width), {operand}),
            mOffset(offset), mWidth(width)
    {}

protected:
    virtual Expr* withOps(std::vector<ExprPtr> ops) const override {
        auto& intTy = *llvm::dyn_cast<IntType>(&getType());
        return new ExtractExpr(ops[0], mOffset, mWidth);
    }

public:
    unsigned getExtractedWidth() const {
        return llvm::dyn_cast<IntType>(&getType())->getWidth();
    }

    unsigned getOffset() const { return mOffset; }
    unsigned getWidth() const { return mWidth; }

    static std::shared_ptr<ExtractExpr> Create(ExprPtr operand, unsigned offset, unsigned width) {
        auto opTy = llvm::dyn_cast<IntType>(&operand->getType());
        assert(opTy != nullptr && "Can only do bitwise cast on integers");
        assert(width > 0 && "Can only extract at least one bit");
        assert(opTy->getWidth() > width + offset && "Extracted bitvector must be smaller than the original");

        auto& resTy = *IntType::get(width);

        return std::shared_ptr<ExtractExpr>(new ExtractExpr(operand, offset, width));
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

/**
 * Casts a pointer to a pointer of another type (e.g. Int8* to Int32*).
 */
class PtrCastExpr final : public UnaryExpr
{
    PtrCastExpr(ExprPtr operand, const PointerType& type)
        : UnaryExpr(Expr::PtrCast, type, {operand})
    {}

protected:
    virtual Expr* withOps(std::vector<ExprPtr> ops) const override {
        auto& ty = *llvm::dyn_cast<PointerType>(&getType());
        return new PtrCastExpr(ops[0], ty);
    }

public:
    static std::shared_ptr<PtrCastExpr> Create(ExprPtr operand, const PointerType& type) {
        assert(operand->getType().isPointerType() && "Can only do pointer cast on pointers");
        
        auto lhsTy = llvm::dyn_cast<PointerType>(&operand->getType());
        auto rhsTy = llvm::dyn_cast<PointerType>(&type);

        return std::shared_ptr<PtrCastExpr>(new PtrCastExpr(operand, *rhsTy));
    }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::PtrCast;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Expr::PtrCast;
    }
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
    virtual Expr* withOps(std::vector<ExprPtr> ops) const override {
        return new ArithmeticExpr<Kind>(getType(), ops[0], ops[1]);
    }

public:
    static std::shared_ptr<ArithmeticExpr<Kind>> Create(ExprPtr left, ExprPtr right)
    {
        assert(left->getType().isIntType() && "Can only define arithmetic operations on integers.");
        assert(left->getType() == right->getType() && "Arithmetic expression operand types must match.");

        return std::shared_ptr<ArithmeticExpr<Kind>>(new ArithmeticExpr<Kind>(left->getType(), left, right));
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
using DivExpr = ArithmeticExpr<Expr::Div>;
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
        : BinaryExpr(Kind, *BoolType::get(), left, right)
    {
        assert(left->getType() == right->getType()
            && "Compare expression operand types must match.");
    }

    virtual Expr* withOps(std::vector<ExprPtr> ops) const override {
        return new CompareExpr(ops[0], ops[1]);
    }

public:
    static std::shared_ptr<CompareExpr<Kind>> Create(ExprPtr left, ExprPtr right) {
        return std::shared_ptr<CompareExpr<Kind>>(new CompareExpr(left, right));
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
        : NonNullaryExpr(Kind, *BoolType::get(), begin, end)
    {
        for (auto it = begin; it != end; ++it) {
            if (!((*it)->getType().isBoolType())) {
                throw TypeCastError("Logic expression operands can only be booleans.");
            }
        }
    }

    MultiaryLogicExpr(ExprPtr left, ExprPtr right)
        : NonNullaryExpr(Kind, *BoolType::get(), {left, right})
    {
        assert(left->getType().isBoolType() && "Logic expression operands can only be booleans.");
        assert((left->getType() == right->getType()) && "Logic expression operand types must match.");
    }

    virtual Expr* withOps(std::vector<ExprPtr> ops) const override {
        return new MultiaryLogicExpr<Kind>(ops[0], ops[1]);
    }
public:
    static std::shared_ptr<MultiaryLogicExpr<Kind>> Create(ExprPtr left, ExprPtr right) {
        return std::shared_ptr<MultiaryLogicExpr<Kind>>(new MultiaryLogicExpr(left, right));
    }

    template<class InputIterator>
    static std::shared_ptr<MultiaryLogicExpr<Kind>> Create(InputIterator begin, InputIterator end) {
        return std::shared_ptr<MultiaryLogicExpr<Kind>>(new MultiaryLogicExpr(begin, end));
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
        : BinaryExpr(Expr::Xor, *BoolType::get(), left, right)
    {}

protected:
    virtual Expr* withOps(std::vector<ExprPtr> ops) const override {
        return new XorExpr(ops[0], ops[1]);
    }

public:
    static std::shared_ptr<XorExpr> Create(ExprPtr left, ExprPtr right)
    {
        assert(left->getType().isBoolType() && "Can only XOR boolean expressions.");
        assert(right->getType().isBoolType() && "Can only XOR boolean expressions.");
        
        return std::shared_ptr<XorExpr>(new XorExpr(left, right));
    }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::Xor;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Expr::Xor;
    }
};

class SelectExpr final : public NonNullaryExpr
{
protected:
    SelectExpr(const Type& type, ExprPtr condition, ExprPtr then, ExprPtr elze)
        : NonNullaryExpr(Expr::Select, type, {condition, then, elze})
    {}

    virtual Expr* withOps(std::vector<ExprPtr> ops) const override {
        assert(ops[0]->getType().isBoolType() && "Select expression condition type must be boolean.");
        assert(ops[1]->getType() == ops[2]->getType() && "Select expression operand types must match.");
        assert(ops[1]->getType() == getType() && "withOps() can only construct ");
        return new SelectExpr(getType(), ops[0], ops[1], ops[2]);
    }
public:
    static std::shared_ptr<SelectExpr> Create(ExprPtr condition, ExprPtr then, ExprPtr elze);

    ExprPtr getCondition() const { return getOperand(0); }
    ExprPtr getThen() const { return getOperand(1); }
    ExprPtr getElse() const { return getOperand(2); }

    /**
     * Type inquiry support.
     */
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
    ArrayReadExpr(std::shared_ptr<VarRefExpr> array, ExprPtr index)
        : NonNullaryExpr(Expr::ArrayRead, array->getType(), {array, index})
    {}
public:
    virtual Expr* withOps(std::vector<ExprPtr> ops) const override;

    std::shared_ptr<VarRefExpr> getArrayRef() const {
        return std::static_pointer_cast<VarRefExpr>(getOperand(0));
    }

    ExprPtr getIndex() const { return getOperand(1); }

    static std::shared_ptr<ArrayReadExpr> Create(std::shared_ptr<VarRefExpr> array, ExprPtr index);

    /**
     * Type inquiry support.
     */
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
    ArrayWriteExpr(std::shared_ptr<VarRefExpr> array, ExprPtr index, ExprPtr value)
        : NonNullaryExpr(Expr::ArrayRead, array->getType(), {array, index, value})
    {}
public:
    virtual Expr* withOps(std::vector<ExprPtr> ops) const override;

    std::shared_ptr<VarRefExpr> getArrayRef() const {
        return std::static_pointer_cast<VarRefExpr>(getOperand(0));
    }
    ExprPtr getIndex() const { return getOperand(1); }
    ExprPtr getElementValue() const { return getOperand(2); }

    static std::shared_ptr<ArrayWriteExpr> Create(
        std::shared_ptr<VarRefExpr> array, ExprPtr index, ExprPtr value
    );

    /**
     * Type inquiry support.
     */
    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::ArrayWrite;
    }
    static bool classof(const Expr& expr) {
        return expr.getKind() == Expr::ArrayWrite;
    }
};

}

#endif
