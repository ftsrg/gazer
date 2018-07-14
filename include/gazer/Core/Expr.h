#ifndef _GAZER_CORE_EXPR_H
#define _GAZER_CORE_EXPR_H

#include "gazer/Core/Type.h"

#include <memory>
#include <string>
#include <iosfwd>

namespace gazer
{

/**
 * Base class for all gazer expressions.
 */
class Expr
{
public:
    enum ExprKind
    {
        // Nullary
        Undef = 0,
        Literal,
        VarRef,

        // Unary logic
        Not,

        // Cast
        ZExt,
        SExt,
        Trunc,

        // Binary arithmetic
        Add,
        Sub,
        Mul,
        Div,

        // Bit operations
        Shl,
        LShr,
        AShr,
        BAnd,
        BOr,
        BXor,

        // Binary logic
        And,
        Or,
        Xor,

        // Compare
        Eq,
        NotEq,
        SLt,
        SLtEq,
        SGt,
        SGtEq,
        ULt,
        ULtEq,
        UGt,
        UGtEq,

        // Ternary
        Select
    };

    static constexpr int FirstUnary = Not;
    static constexpr int LastUnary = SExt;
    static constexpr int FirstUnaryCast = ZExt;
    static constexpr int LastUnaryCast = Trunc;
    static constexpr int FirstBinaryArithmetic = Add;
    static constexpr int LastBinaryArithmetic = BXor;
    static constexpr int FirstLogic = And;
    static constexpr int LastLogic = Xor;
    static constexpr int FirstCompare = Eq;
    static constexpr int LastCompare = UGtEq;

    static constexpr int FirstExprKind = Undef;
    static constexpr int LastExprKind = Select;

protected:
    Expr(ExprKind kind, const Type& type)
        : mKind(kind), mType(type)
    {}

public:
    ExprKind getKind() const { return mKind; }
    const Type& getType() const { return mType; }

    bool isNullary() const { return mKind <= FirstUnary; }
    bool isUnary() const { return FirstUnary <= mKind && mKind <= LastUnary; }

    bool isArithmetic() const {
        return FirstBinaryArithmetic <= mKind && mKind <= LastBinaryArithmetic;
    }
    bool isLogic() const {
        return FirstLogic <= mKind && mKind <= LastLogic;
    }
    bool isCompare() const {
        return FirstCompare <= mKind && mKind <= LastCompare;
    }

    virtual void print(std::ostream& os) const = 0;

    virtual ~Expr() {}

public:
    static std::string getKindName(ExprKind kind);

protected:
    const ExprKind mKind;
    const Type& mType;
};

using ExprPtr = std::shared_ptr<Expr>;
using ExprVector = std::vector<ExprPtr>;

std::ostream& operator<<(std::ostream& os, const Expr& expr);

/**
 * Base class for all literals.
 */
class LiteralExpr : public Expr
{
protected:
    LiteralExpr(const Type& type)
        : Expr(Literal, type)
    {}
public:
    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal;
    }
};

/**
 * Base class for all expressions holding one or more operands.
 */
class NonNullaryExpr : public Expr
{
protected:
    NonNullaryExpr(ExprKind kind, const Type& type, std::vector<ExprPtr> ops)
        : NonNullaryExpr(kind, type, ops.begin(), ops.end())
    {}

    template<class InputIterator>
    NonNullaryExpr(ExprKind kind, const Type& type, InputIterator begin, InputIterator end)
        : Expr(kind, type), mOperands(begin, end)
    {
        assert(mOperands.size() >= 1 && "Non-nullary expressions must have at least one operand.");
    }

protected:
    virtual Expr* withOps(std::vector<ExprPtr> ops) const = 0;

public: 
    virtual void print(std::ostream& os) const override;

    template<class Iter>
    Expr* with(Iter begin, Iter end) {
        // Check if all operands are the same
        bool equals = true;
        auto it = begin;
        auto opIt = op_begin();

        size_t itCount = 0;
        while (it != end && opIt != op_end()) {
            assert(*it != nullptr && "nullptr in NonNullaryExpr::with()");
            if (*it != *opIt) {
                equals = false;
                // Cannot break here, because we need
                // to update the itCount variable.
            }
            ++itCount;
            ++it, ++opIt;
        }

        if (equals) {
            // The operands are the same, we can just return this instance
            return this;
        }
        
        assert(itCount == getNumOperands()
            && "NonNullaryExpr::with() operand counts must match");

        // Operand types must match to the operands of this class
        return this->withOps({begin, end});
    }

    //---- Operand handling ----//
    using op_iterator = typename std::vector<ExprPtr>::iterator;
    using op_const_iterator = typename std::vector<ExprPtr>::const_iterator;

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

    size_t getNumOperands() const { return mOperands.size(); }
    ExprPtr getOperand(size_t idx) const { return mOperands[idx]; }

public:
    static bool classof(const Expr* expr) {
        return expr->getKind() >= FirstUnary;
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() >= FirstUnary;
    }

private:
    static ExprPtr CreateSubClass(ExprKind kind, std::vector<ExprPtr> ops);

private:
    std::vector<ExprPtr> mOperands;
};

}

#endif
