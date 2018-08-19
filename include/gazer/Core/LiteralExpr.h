#ifndef _GAZER_CORE_LITERALEXPR_H
#define _GAZER_CORE_LITERALEXPR_H

#include "gazer/Core/Expr.h"

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APFloat.h>

namespace gazer
{

class UndefExpr final : public Expr
{
private:
    UndefExpr(const Type& type)
        : Expr(Expr::Undef, type)
    {}
public:
    virtual void print(llvm::raw_ostream& os) const override;
    static std::shared_ptr<UndefExpr> Get(const Type& type);
};

class BoolLiteralExpr final : public LiteralExpr
{
private:
    BoolLiteralExpr(bool value)
        : LiteralExpr(*BoolType::get()), mValue(value)
    {}

public:
    static std::shared_ptr<BoolLiteralExpr> getTrue();
    static std::shared_ptr<BoolLiteralExpr> getFalse();

    static std::shared_ptr<BoolLiteralExpr> Get(bool value) {
        return value ? getTrue() : getFalse();
    }

    virtual void print(llvm::raw_ostream& os) const override;
    bool getValue() const { return mValue; }

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
    IntLiteralExpr(const IntType& type, llvm::APInt value)
        : LiteralExpr(type), mValue(value)
    {
        assert(type.getWidth() == value.getBitWidth() && "Type and literal bit width must match.");
    }
public:
    virtual void print(llvm::raw_ostream& os) const override;

public:
    static std::shared_ptr<IntLiteralExpr> get(IntType& type, llvm::APInt value);

    llvm::APInt getValue() const { return mValue; }

    const IntType& getType() const {
        return static_cast<const IntType&>(mType);
    }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isIntType();
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Literal && expr.getType().isIntType();
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

    static std::shared_ptr<FloatLiteralExpr> get(const FloatType& type, const llvm::APFloat& value);

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

}

#endif
