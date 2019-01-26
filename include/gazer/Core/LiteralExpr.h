#ifndef _GAZER_CORE_LITERALEXPR_H
#define _GAZER_CORE_LITERALEXPR_H

#include "gazer/Core/Expr.h"

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APFloat.h>

namespace llvm {
    class ConstantData;
}

namespace gazer
{

class UndefExpr final : public AtomicExpr
{
private:
    UndefExpr(const Type& type)
        : AtomicExpr(Expr::Undef, type)
    {}
public:
    static ExprRef<UndefExpr> Get(const Type& type);
    virtual void print(llvm::raw_ostream& os) const override;

    static bool classof(const Expr* expr) {
        return expr->getKind() == Undef;
    }
};

class BoolLiteralExpr final : public LiteralExpr
{
private:
    BoolLiteralExpr(bool value)
        : LiteralExpr(BoolType::get()), mValue(value)
    {}

public:
    static ExprRef<BoolLiteralExpr> getTrue();
    static ExprRef<BoolLiteralExpr> getFalse();

    static ExprRef<BoolLiteralExpr> Get(bool value) {
        return value ? getTrue() : getFalse();
    }

    virtual void print(llvm::raw_ostream& os) const override;
    virtual bool equals(const LiteralExpr& other) const override;

    bool getValue() const { return mValue; }
    bool isTrue() const { return mValue == true; }
    bool isFalse() const { return mValue == false; }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isBoolType();
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Literal && expr.getType().isBoolType();
    }
private:
    bool mValue;
};

class IntLiteralExpr final : public LiteralExpr
{
private:
    IntLiteralExpr(const IntType& type, int64_t value)
        : LiteralExpr(type), mValue(value)
    {}

public:
    static ExprRef<IntLiteralExpr> get(IntType& type, int64_t value);

public:
    virtual void print(llvm::raw_ostream& os) const override;
    virtual bool equals(const LiteralExpr& other) const override;

    int64_t getValue() const { return mValue; }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isIntType();
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Literal && expr.getType().isIntType();
    }
private:
    int64_t mValue;
};

class BvLiteralExpr final : public LiteralExpr
{
private:
    BvLiteralExpr(BvType& type, llvm::APInt value)
        : LiteralExpr(type), mValue(value)
    {
        assert(type.getWidth() == value.getBitWidth() && "Type and literal bit width must match.");
    }
public:
    virtual void print(llvm::raw_ostream& os) const override;
    virtual bool equals(const LiteralExpr& other) const override;

public:
    static ExprRef<BvLiteralExpr> Get(llvm::APInt value);

    llvm::APInt getValue() const { return mValue; }

    const BvType& getType() const {
        return static_cast<const BvType&>(mType);
    }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isBvType();
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Literal && expr.getType().isBvType();
    }

private:
    //uint64_t mValue;
    llvm::APInt mValue;
};

class FloatLiteralExpr final : public LiteralExpr
{
private:
    FloatLiteralExpr(const FloatType& type, const llvm::APFloat& value)
        : LiteralExpr(type), mValue(value)
    {}
public:
    virtual void print(llvm::raw_ostream& os) const override;
    virtual bool equals(const LiteralExpr& other) const override;

    static ExprRef<FloatLiteralExpr> get(const FloatType& type, const llvm::APFloat& value);
    static ExprRef<FloatLiteralExpr> get(FloatType::FloatPrecision precision, const llvm::APFloat& value);

    llvm::APFloat getValue() const { return mValue; }

    const FloatType& getType() const {
        return static_cast<const FloatType&>(mType);
    }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isFloatType();
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Literal && expr.getType().isFloatType();
    }

private:
    llvm::APFloat mValue;
};

/**
 * Transforms an LLVM constant into a LiteralExpr.
 * 
 * @param value The value to transform.
 * @param i1AsBool Treat constants of i1 type as booleans.
 */
ExprRef<LiteralExpr> LiteralFromLLVMConst(
    llvm::ConstantData* value,
    bool i1AsBool = true
);

namespace detail
{

template<Type::TypeID TypeID> struct GetLiteralExprTypeHelper {};

template<>
struct GetLiteralExprTypeHelper<Type::BoolTypeID> { using T = BoolLiteralExpr; };
template<>
struct GetLiteralExprTypeHelper<Type::IntTypeID> { using T = IntLiteralExpr; };
template<>
struct GetLiteralExprTypeHelper<Type::BvTypeID> { using T = BvLiteralExpr; };
template<>
struct GetLiteralExprTypeHelper<Type::FloatTypeID> { using T = FloatLiteralExpr; };

} // end namespace detail

/**
 * Helper class that returns the literal expression type for a given Gazer type.
 */
template<Type::TypeID TypeID>
struct GetLiteralExprType
{
    using T = typename detail::GetLiteralExprTypeHelper<TypeID>::T;
};


}

#endif
