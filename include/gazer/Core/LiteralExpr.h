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
    friend class ExprStorage;
private:
    UndefExpr(Type& type)
        : AtomicExpr(Expr::Undef, type)
    {}
public:
    static ExprRef<UndefExpr> Get(Type& type);
    virtual void print(llvm::raw_ostream& os) const override;

    static bool classof(const Expr* expr) {
        return expr->getKind() == Undef;
    }
};

class BoolLiteralExpr final : public LiteralExpr
{
    friend class GazerContextImpl;
private:
    BoolLiteralExpr(BoolType& type, bool value)
        : LiteralExpr(type), mValue(value)
    {}

public:
    static ExprRef<BoolLiteralExpr> True(BoolType& type);
    static ExprRef<BoolLiteralExpr> True(GazerContext& context) { return True(BoolType::Get(context)); }

    static ExprRef<BoolLiteralExpr> False(BoolType& type);
    static ExprRef<BoolLiteralExpr> False(GazerContext& context) { return False(BoolType::Get(context)); }

    static ExprRef<BoolLiteralExpr> Get(GazerContext& context, bool value) {
        return Get(BoolType::Get(context), value);
    }
    static ExprRef<BoolLiteralExpr> Get(BoolType& type, bool value) {
        return value ? True(type) : False(type);
    }

    void print(llvm::raw_ostream& os) const override;

    bool getValue() const { return mValue; }
    bool isTrue() const { return mValue == true; }
    bool isFalse() const { return mValue == false; }

public:
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
    friend class ExprStorage;
private:
    IntLiteralExpr(IntType& type, int64_t value)
        : LiteralExpr(type), mValue(value)
    {}

public:
    static ExprRef<IntLiteralExpr> Get(IntType& type, int64_t value);

public:
    virtual void print(llvm::raw_ostream& os) const override;

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
    friend class ExprStorage;
    friend class GazerContextImpl;
private:
    BvLiteralExpr(BvType& type, llvm::APInt value)
        : LiteralExpr(type), mValue(value)
    {
        assert(type.getWidth() == value.getBitWidth() && "Type and literal bit width must match.");
    }
public:
    virtual void print(llvm::raw_ostream& os) const override;

public:
    static ExprRef<BvLiteralExpr> Get(BvType& type, llvm::APInt value);

    llvm::APInt getValue() const { return mValue; }

    BvType& getType() const {
        return static_cast<BvType&>(mType);
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
    friend class ExprStorage;
private:
    FloatLiteralExpr(FloatType& type, llvm::APFloat value)
        : LiteralExpr(type), mValue(value)
    {}
public:
    void print(llvm::raw_ostream& os) const override;

    static ExprRef<FloatLiteralExpr> Get(FloatType& type, const llvm::APFloat& value);

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

ExprRef<LiteralExpr> LiteralFromLLVMConst(
    GazerContext& context,
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
