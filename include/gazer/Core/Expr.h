//==- Expr.h - Core expression classes --------------------------*- C++ -*--==//
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
///
/// \file This file defines the different base classes for expressions.
/// For implementing subclasses, see gazer/Core/ExprTypes.h and
/// gazer/Core/LiteralExpr.h.
///
//===----------------------------------------------------------------------===//
#ifndef GAZER_CORE_EXPR_H
#define GAZER_CORE_EXPR_H

#include "gazer/Core/Type.h"
#include "gazer/Core/Decl.h"
#include "gazer/Core/ExprRef.h"

#include <llvm/ADT/StringRef.h>

#include <boost/intrusive_ptr.hpp>

#include <memory>
#include <string>

namespace llvm {
    class raw_ostream;
}

namespace gazer
{

class GazerContextImpl;

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
    // If you wish to add a new expression type, make sure to do the following:
    //      (1) Update ExprKind.inc with the new kind.
    //      (2) Update ExprKindPrimes in Expr.cpp with a new unique prime number.
    //      (3) Create an implementation class. If you use a template, explicitly
    //          instantiate it in Expr.cpp.
    //      (4) If your implementation class is atomic or a non-trivial descendant of 
    //          NonNullaryExpr, update expr_hasher in GazerContextImpl.h with a specialization
    //          for your implementation.
    //      (5) Update the ExprWalker interface. Note that this also means the possible
    //          update of their implementations (such as solvers).
    //  Things will work without the following changes, but they are highly recommended:
    //      (6) Add a corresponding method to ExprBuilder and ConstantFolder.
    enum ExprKind
    {
        // Nullary
        Undef = 0,
        Literal,
        VarRef,

        // Unary logic
        Not,

        // Cast
        ZExt,    ///< zero extend to another type
        SExt,    ///< sign extend to another type
        Extract,

        // Binary arithmetic
        Add,
        Sub,
        Mul,
        Div,    ///< division operator for arithmetic types
        Mod,    ///< modulo operator for arithmetic types
        Rem,    ///< remainder operator for arithmetic types
        BvSDiv, ///< signed division for bitvectors
        BvUDiv, ///< unsigned division for bitvectors
        BvSRem, ///< signed remainder for bitvectors
        BvURem, ///< unsigned remainder for bitvectors

        // Bitvector operations
        Shl,        ///< binary shift left
        LShr,       ///< logical shift right
        AShr,       ///< arithmetic shift right
        BvAnd,      ///< binary AND for bitvectors
        BvOr,       ///< binary OR for bitvectors
        BvXor,      ///< binary XOR for bitvectors
        BvConcat,   ///< bitvector concatenation

        // Binary logic
        And,    ///< multiary AND operator for booleans
        Or,     ///< multiary OR operator for booleans
        Xor,    ///< binary XOR operator for booleans
        Imply,  ///< binary implication for booleans

        // Compare
        Eq,
        NotEq,
        Lt,     ///< arithmetic less than
        LtEq,   ///< arithmetic less than or equal
        Gt,     ///< arithmetic greater than
        GtEq,   ///< arithmetic greater than or equal

        // Bitvector compare
        BvSLt,      ///< bitvector signed less than or equal
        BvSLtEq,    ///< bitvector signed less than or equal
        BvSGt,      ///< bitvector signed greater than
        BvSGtEq,    ///< bitvector signed greater than or equal
        BvULt,      ///< bitvector unsigned less than
        BvULtEq,    ///< bitvector unsigned less than or equal
        BvUGt,      ///< bitvector unsigned greater than
        BvUGtEq,    ///< bitvector unsigned greater than or equal
        
        // Floating point unary
        FIsNan,
        FIsInf,

        // Floating point cast
        FCast,
        SignedToFp,
        UnsignedToFp,
        FpToSigned,
        FpToUnsigned,

        // Floating point binary
        FAdd,
        FSub,
        FMul,
        FDiv,

        // Floating point compare
        // All comparisons evaluate to false if either operand is NaN
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

        // Tuples
        TupleSelect,
        TupleConstruct
    };

    // Atomic and literal expressions
    static constexpr int FirstAtomic = Undef;
    static constexpr int LastAtomic = Literal;

    // Unary operations and casts
    static constexpr int FirstUnary = Not;
    static constexpr int LastUnary = Extract;
    static constexpr int FirstUnaryCast = ZExt;
    static constexpr int LastUnaryCast = Extract;
    
    // Binary operations
    static constexpr int FirstBinaryArithmetic = Add;
    static constexpr int LastBinaryArithmetic = BvConcat;
    static constexpr int FirstBitLogic = Shl;
    static constexpr int LastBitLogic  = BvConcat;

    // Logic and compare
    static constexpr int FirstLogic = And;
    static constexpr int LastLogic = Xor;
    static constexpr int FirstCompare = Eq;
    static constexpr int LastCompare = BvUGtEq;

    // Floats
    static constexpr int FirstFp = FIsNan;
    static constexpr int LastFp = FLtEq;

    static constexpr int FirstFpUnary = FIsNan;
    static constexpr int LastFpUnary = FpToUnsigned;
    static constexpr int FirstFpArithmetic = FAdd;
    static constexpr int LastFpArithmetic = FDiv;
    static constexpr int FirstFpCompare = FEq;
    static constexpr int LastFpCompare = FLtEq;

    // Generic expressions
    static constexpr int FirstExprKind = Undef;
    static constexpr int LastExprKind = TupleConstruct;

protected:
    Expr(ExprKind kind, Type& type);

public:
    Expr(const Expr&) = delete;
    Expr& operator=(const Expr&) = delete;

    ExprKind getKind() const { return mKind; }
    Type& getType() const { return mType; }
    GazerContext& getContext() const { return mType.getContext(); }

    bool isUndef() const { return mKind == Undef; }

    bool isNullary() const { return mKind < FirstUnary; }
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

    bool isBitLogic() const {
        return FirstBitLogic <= mKind && mKind <= LastBitLogic;
    }

    bool isCompare() const {
        return FirstCompare <= mKind && mKind <= LastCompare;
    }

    bool isFloatingPoint() const {
        return FirstFp <= mKind && mKind <= LastFp;
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
    virtual ~Expr() = default;

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

class VarRefExpr final : public Expr
{
    friend class Variable;
    friend class ExprStorage;
private:
    explicit VarRefExpr(Variable* variable);

public:
    Variable& getVariable() const { return *mVariable; }

    void print(llvm::raw_ostream& os) const override;

    static bool classof(const Expr* expr) {
        return expr->getKind() == Expr::VarRef;
    }

private:
    Variable* mVariable;
};

/// Base class for all expressions holding one or more operands.
class NonNullaryExpr : public Expr
{
    friend class ExprStorage;
protected:
    template<class InputIterator>
    NonNullaryExpr(ExprKind kind, Type& type, InputIterator begin, InputIterator end)
        : Expr(kind, type), mOperands(begin, end)
    {
        assert(!mOperands.empty() && "Non-nullary expressions must have at least one operand.");
        assert(std::none_of(begin, end, [](const ExprPtr& elem) { return elem == nullptr; })
            && "Non-nullary expression operands cannot be null!"
        );
    }

public: 
    void print(llvm::raw_ostream& os) const override;

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

template<class T>
struct hash<gazer::ExprRef<T>>
{
    size_t operator()(const gazer::ExprRef<T>& expr) const {
        return std::hash<gazer::Expr*>{}(expr.get());
    }
};

} // end namespace std

#endif
