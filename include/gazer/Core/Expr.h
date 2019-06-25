/// \file This file defines the different base classes for expressions.
/// For implementing classes, see gazer/Core/ExprTypes.h,
/// gazer/Core/LiteralExpr.h, and gazer/Core/Variable.h
#ifndef _GAZER_CORE_EXPR_H
#define _GAZER_CORE_EXPR_H

#include "gazer/Core/Type.h"

#include <llvm/ADT/StringRef.h>

#include <boost/intrusive_ptr.hpp>

#include <memory>
#include <string>

namespace llvm {
    class raw_ostream;
}

namespace gazer
{

class Expr;
class GazerContext;
class GazerContextImpl;

/// Intrusive reference counting pointer for expression types.
template<class T = Expr> using ExprRef = boost::intrusive_ptr<T>;

using ExprPtr = ExprRef<Expr>;

/// \brief Base class for all gazer expressions.
///
/// Expression subclass constructors are private. The intended way of 
/// instantiation is by using the static ::Create() functions
/// of the subclasses or using an ExprBuilder.
class Expr
{
    friend class ExprStorage;
    friend class GazerContextImpl;
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
        ZExt,       ///< zero extend to another type
        SExt,       ///< sign extend to another type
        Extract,

        // Binary arithmetic
        Add,
        Sub,
        Mul,
        SDiv,
        UDiv,
        SRem,
        URem,

        // Bit operations
        Shl,    ///< binary shift left
        LShr,   ///< logical shift right
        AShr,   ///< arithmetic shift right
        BAnd,   ///< binary AND for bit vectors
        BOr,    ///< binary OR for bit vectors
        BXor,   ///< binary XOR for bit vectors

        // Binary logic
        And,    ///< multiary AND operator for booleans
        Or,     ///< multiary OR operator for booleans
        Xor,    ///< binary XOR operator for booleans
        Imply,  ///< binary implication for booleans

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
        
        // Floating point unary
        FIsNan,
        FIsInf,

        // Floating point cast
        // FpExt,
        // FpTrunc,
        // SignedToFp,
        // UnsignedToFp,
        // FpToSigned,
        // FpToUnsigned,

        // Floating point binary
        FAdd,
        FSub,
        FMul,
        FDiv,

        // Floating point compare
        FEq,
        FGt,
        FGtEq,
        FLt,
        FLtEq,

        // Ternary
        Select, ///< ternary if-then-else (ITE) operator

        // Array operations
        ArrayRead,
        ArrayWrite,
    };

    static constexpr int FirstAtomic = Undef;
    static constexpr int LastAtomic = Literal;

    static constexpr int FirstUnary = Not;
    static constexpr int LastUnary = Extract;
    static constexpr int FirstUnaryCast = ZExt;
    static constexpr int LastUnaryCast = Extract;
    static constexpr int FirstBinaryArithmetic = Add;
    static constexpr int LastBinaryArithmetic = BXor;
    static constexpr int FirstLogic = And;
    static constexpr int LastLogic = Xor;
    static constexpr int FirstCompare = Eq;
    static constexpr int LastCompare = UGtEq;
    static constexpr int FirstFpUnary = FIsNan;
    static constexpr int LastFpUnary = FIsInf;
    static constexpr int FirstFpArithmetic = FAdd;
    static constexpr int LastFpArithmetic = FDiv;
    static constexpr int FirstFpCompare = FEq;
    static constexpr int LastFpCompare = FLtEq;

    static constexpr int FirstExprKind = Undef;
    static constexpr int LastExprKind = ArrayWrite;

protected:
    Expr(ExprKind kind, Type& type);

public:
    Expr(const Expr&) = delete;
    Expr& operator=(const Expr&) = delete;

    ExprKind getKind() const { return mKind; }
    Type& getType() const { return mType; }
    GazerContext& getContext() const { return mType.getContext(); }

    bool isNullary() const { return mKind <= FirstUnary; }
    bool isUnary() const {
        return (FirstUnary <= mKind && mKind <= LastUnary)
            || (FirstFpUnary <= mKind && mKind <= LastFpUnary);
    }

    bool isArithmetic() const {
        return FirstBinaryArithmetic <= mKind && mKind <= LastBinaryArithmetic;
    }

    bool isLogic() const {
        return FirstLogic <= mKind && mKind <= LastLogic;
    }

    bool isCompare() const {
        return FirstCompare <= mKind && mKind <= LastCompare;
    }

    bool isFpArithmetic() const {
        return FirstFpArithmetic <= mKind && mKind <= LastFpArithmetic;
    }

    bool isFpCompare() const {
        return FirstFpCompare <= mKind && mKind <= LastFpCompare;
    }

    bool hasSubclassData() const {
        return mKind == Extract || this->isFpArithmetic();
    }

    /// Calculates a hash code for this expression.
    std::size_t getHashCode() const;

    virtual void print(llvm::raw_ostream& os) const = 0;
    virtual ~Expr() {}

public:
    static llvm::StringRef getKindName(ExprKind kind);

private:
    static void DeleteExpr(Expr* expr);

    friend void intrusive_ptr_add_ref(Expr* expr) {
        expr->mRefCount++;
    }

    friend void intrusive_ptr_release(Expr* expr) {
        assert(expr->mRefCount > 0 && "Attempting to decrease a zero ref counter!");
        if (--expr->mRefCount == 0) {
            Expr::DeleteExpr(expr);
        }
    }

protected:
    const ExprKind mKind;
    Type& mType;

private:
    mutable unsigned mRefCount;
    Expr* mNextPtr = nullptr;
    mutable size_t mHashCode = 0;
};

using ExprVector = std::vector<ExprPtr>;

template<class T = Expr>
inline ExprRef<T> make_expr_ref(T* expr) {
    return ExprRef<T>(expr);
}

template<class ToT, class FromT>
inline ExprRef<ToT> expr_cast(const ExprRef<FromT>& value) {
    return llvm::isa<ToT>(value.get()) ? boost::static_pointer_cast<ToT>(value) : nullptr;
}

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const Expr& expr);

/// Expression base class for atomic expression values.
/// Currently literals and undef values are considered as atomic.
class AtomicExpr : public Expr
{
protected:
    AtomicExpr(Expr::ExprKind kind, Type& type)
        : Expr(kind, type)
    {}
public:
    static bool classof(const Expr* expr) {
        return expr->getKind() >= FirstAtomic && expr->getKind() <= LastAtomic;
    }
};

/// \brief Expression base class of literal expressions.
/// 
/// Note that while Expr::Literal is used to indicate
/// that an expression is literal, this class is abstract.
/// To acquire the value stored in literals, use the literal
/// subclasses (BvLiteralExpr, ...).
class LiteralExpr : public AtomicExpr
{
protected:
    explicit LiteralExpr(Type& type)
        : AtomicExpr(Literal, type)
    {}
public:
    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal;
    }
};

/// Base class for all expressions holding one or more operands.
class NonNullaryExpr : public Expr
{
    friend class ExprStorage;
protected:
    NonNullaryExpr(ExprKind kind, Type& type, const ExprVector& ops)
        : NonNullaryExpr(kind, type, ops.begin(), ops.end())
    {}

    template<class InputIterator>
    NonNullaryExpr(ExprKind kind, Type& type, InputIterator begin, InputIterator end)
        : Expr(kind, type), mOperands(begin, end)
    {
        assert(mOperands.size() >= 1 && "Non-nullary expressions must have at least one operand.");
        assert(std::none_of(begin, end, [](ExprPtr elem) { return elem == nullptr; })
            && "Non-nullary expression operands cannot be null!"
        );
    }

protected:

    virtual ExprPtr cloneImpl(ExprVector ops) const = 0;

public: 
    void print(llvm::raw_ostream& os) const override;

    /// Creates a clone of this expression, with the operands in \param ops.
    /// If the operand list of the given expression and the \param ops vector are the same,
    /// this function returns the original expression.
    ExprPtr clone(ExprVector ops);

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
    std::vector<ExprPtr> mOperands;
};

} // end namespace gazer

// Add support for llvm-related stuff
namespace llvm
{

template<class T>
struct simplify_type<gazer::ExprRef<T>> {
    typedef T* SimpleType;
    static SimpleType getSimplifiedValue(gazer::ExprRef<T> &Val) { return Val.get(); }
};

template<class T>
struct simplify_type<const gazer::ExprRef<T>> {
    typedef T* SimpleType;
    static SimpleType getSimplifiedValue(const gazer::ExprRef<T> &Val) { return Val.get(); }
};

} // end namespace llvm

namespace std
{

template<>
struct hash<gazer::ExprPtr>
{
    size_t operator()(const gazer::ExprPtr& expr) const {
        return std::hash<gazer::Expr*>{}(expr.get());
    }
};

} // end namespace std

#endif
