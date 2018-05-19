#include "gazer/Core/Expr.h"

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/iterator_range.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/Casting.h>

#include <cassert>
#include <array>

namespace gazer
{

/**
 * Base class for all expressions holding one or more operands.
 */
template<unsigned N>
class NonNullaryExpr : public Expr
{
protected:
    NonNullaryExpr(ExprKind kind, const Type& type, std::array<ExprPtr, N> operands)
        : Expr(kind, type), mOperands(operands)
    {}
public:
    using op_iterator = typename std::array<ExprPtr, N>::iterator;
    using op_const_iterator = typename std::array<ExprPtr, N>::const_iterator;

    op_iterator op_begin() { return mOperands.begin(); }
    op_iterator op_end() { return mOperands.end(); }

    op_const_iterator op_begin() const { return mOperands.begin(); }
    op_const_iterator op_end() const { return mOperands.end(); }

    llvm::iterator_range<op_iterator> operands() {
        return llvm::make_range(op_begin(), op_end());
    }
    llvm::iterator_range<op_const_iterator> operands() const {
        return llvm::make_range(op_begin(), op_end());
    }

    ExprPtr getOperand(size_t idx) const { return mOperands[idx]; }

    static bool classof(const Expr* expr) {
        return expr->getKind() >= FirstUnary;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() >= FirstUnary;
    }

private:
    std::array<ExprPtr, N> mOperands;
};

//============
// Unary expressions
//============

class UnaryExpr : public NonNullaryExpr<1>
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
public:
    static std::shared_ptr<NotExpr> Create(ExprPtr operand)
    {
        assert(operand->getType().isBoolType() && "Can only negate boolean expressions.");
        return std::shared_ptr<NotExpr>(new NotExpr(operand));
    }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::Not;
    }
};

class BinaryExpr : public NonNullaryExpr<2>
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
};

using AddExpr = ArithmeticExpr<Expr::Add>;
using SubExpr = ArithmeticExpr<Expr::Sub>;
using MulExpr = ArithmeticExpr<Expr::Mul>;
using DivExpr = ArithmeticExpr<Expr::Div>;

template<Expr::ExprKind Kind>
class CompareExpr final : public BinaryExpr
{
    static_assert(Expr::FirstCompare <= Kind && Kind <= Expr::LastCompare,
        "A compare expression must have a compare expression kind.");
protected:
    CompareExpr(ExprPtr left, ExprPtr right)
        : BinaryExpr(Kind, *BoolType::get(), left, right)
    {}
public:
    static std::shared_ptr<CompareExpr<Kind>> Create(ExprPtr left, ExprPtr right)
    {
        assert((left->getType() == right->getType()) && "Compare expression operand types must match.");
        return std::shared_ptr<CompareExpr<Kind>>(new CompareExpr(left, right));
    }    
    
    /**
     * Type inquiry support.
     */
    static bool classof(const Expr* expr) {
        return expr->getKind() == Kind;
    }
};

using EqExpr    = CompareExpr<Expr::Eq>;
using NotEqExpr = CompareExpr<Expr::NotEq>;
using LtExpr    = CompareExpr<Expr::Lt>;
using LtEqExpr  = CompareExpr<Expr::LtEq>;
using GtExpr    = CompareExpr<Expr::Gt>;
using GtEqExpr  = CompareExpr<Expr::GtEq>;

template<Expr::ExprKind Kind>
class BinaryLogicExpr final : public BinaryExpr
{
    static_assert(Expr::FirstLogic <= Kind && Kind <= Expr::LastLogic,
        "A logic expression must have a logic expression kind.");
protected:
    BinaryLogicExpr(ExprPtr left, ExprPtr right)
        : BinaryExpr(Kind, *BoolType::get(), left, right)
    {}
public:
    static std::shared_ptr<BinaryLogicExpr<Kind>> Create(ExprPtr left, ExprPtr right)
    {
        assert(left->getType().isBoolType() && "Logic expression operands can only be booleans.");
        assert((left->getType() == right->getType()) && "Logic expression operand types must match.");
        return std::shared_ptr<BinaryLogicExpr<Kind>>(new BinaryLogicExpr(left, right));
    }    
    
    /**
     * Type inquiry support.
     */
    static bool classof(const Expr* expr) {
        return expr->getKind() == Kind;
    }
};

using AndExpr = BinaryLogicExpr<Expr::And>;
using OrExpr  = BinaryLogicExpr<Expr::Or>;
using XorExpr = BinaryLogicExpr<Expr::Xor>;

class SelectExpr final : public NonNullaryExpr<3>
{
protected:
    SelectExpr(const Type& type, ExprPtr condition, ExprPtr then, ExprPtr elze)
        : NonNullaryExpr(Expr::Select, type, {condition, then, elze})
    {}
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
};

}
