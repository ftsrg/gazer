//==- LiteralExpr.h - Literal expression subclasses -------------*- C++ -*--==//
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
#ifndef GAZER_CORE_LITERALEXPR_H
#define GAZER_CORE_LITERALEXPR_H

#include "gazer/Core/Expr.h"

#include <llvm/ADT/APInt.h>
#include <llvm/ADT/APFloat.h>

#include <boost/rational.hpp>

#include <map>

namespace llvm {
    class ConstantData;
}

namespace gazer
{

class UndefExpr final : public AtomicExpr
{
    friend class ExprStorage;
private:
    explicit UndefExpr(Type& type)
        : AtomicExpr(Expr::Undef, type)
    {}
public:
    static ExprRef<UndefExpr> Get(Type& type);
    void print(llvm::raw_ostream& os) const override;

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

    BoolType& getType() const { return static_cast<BoolType&>(mType); }

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
public:
    using ValueTy = int64_t;
private:
    IntLiteralExpr(IntType& type, long long int value)
        : LiteralExpr(type), mValue(value)
    {}

public:
    static ExprRef<IntLiteralExpr> Get(IntType& type, long long int value);
    static ExprRef<IntLiteralExpr> Get(GazerContext& ctx, long long int value) {
        return Get(IntType::Get(ctx), value);
    }

public:
    void print(llvm::raw_ostream& os) const override;

    int64_t getValue() const { return mValue; }

    bool isZero() const { return mValue == 0; }
    bool isOne() const { return mValue == 1; }

    IntType& getType() const { return static_cast<IntType&>(mType); }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isIntType();
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Literal && expr.getType().isIntType();
    }
private:
    long long int mValue;
};

class RealLiteralExpr final : public LiteralExpr
{
    friend class ExprStorage;
private:
    RealLiteralExpr(RealType& type, boost::rational<long long int> value)
        : LiteralExpr(type), mValue(value)
    {}

public:
    static ExprRef<RealLiteralExpr> Get(RealType& type, boost::rational<long long int> value);
    static ExprRef<RealLiteralExpr> Get(RealType& type, long long int num, long long int denom) {
        return Get(type, boost::rational<long long int>(num, denom));
    }

public:
    void print(llvm::raw_ostream& os) const override;

    boost::rational<long long int> getValue() const { return mValue; }

    RealType& getType() const { return static_cast<RealType&>(mType); }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isRealType();
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Literal && expr.getType().isRealType();
    }

private:
    boost::rational<long long int> mValue;
};

class BvLiteralExpr final : public LiteralExpr
{
    friend class ExprStorage;
    friend class GazerContextImpl;
private:
    BvLiteralExpr(BvType& type, llvm::APInt value)
        : LiteralExpr(type), mValue(std::move(value))
    {
        assert(type.getWidth() == mValue.getBitWidth() && "Type and literal bit width must match.");
    }
public:
    void print(llvm::raw_ostream& os) const override;

public:
    static ExprRef<BvLiteralExpr> Get(BvType& type, const llvm::APInt& value);
    static ExprRef<BvLiteralExpr> Get(BvType& type, uint64_t value) {
        return Get(type, llvm::APInt{type.getWidth(), value});
    }

    llvm::APInt getValue() const { return mValue; }

    bool isOne() const { return mValue.isOneValue(); }
    bool isZero() const { return mValue.isNullValue(); }
    bool isAllOnes() const { return mValue.isAllOnesValue(); }

    BvType& getType() const { return static_cast<BvType&>(mType); }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isBvType();
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Literal && expr.getType().isBvType();
    }

private:
    llvm::APInt mValue;
};

class FloatLiteralExpr final : public LiteralExpr
{
    friend class ExprStorage;
private:
    FloatLiteralExpr(FloatType& type, llvm::APFloat value)
        : LiteralExpr(type), mValue(std::move(value))
    {}
public:
    void print(llvm::raw_ostream& os) const override;

    static ExprRef<FloatLiteralExpr> Get(FloatType& type, const llvm::APFloat& value);

    llvm::APFloat getValue() const { return mValue; }

    FloatType& getType() const {
        return static_cast<FloatType&>(mType);
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

class ArrayLiteralExpr final : public LiteralExpr
{
    friend class ExprStorage;
public:
    // We cannot use unordered_map here, as we need to provide
    // a predictable hash function for this container.
    using MappingT = std::map<
        ExprRef<LiteralExpr>,
        ExprRef<LiteralExpr>
    >;

    /// Helper class for building array literals
    class Builder
    {
    public:
        explicit Builder(ArrayType& type)
            : mType(type)
        {}

        void addValue(ExprRef<LiteralExpr> index, ExprRef<LiteralExpr> element);
        void setDefault(const ExprRef<LiteralExpr>& expr);

        ExprRef<ArrayLiteralExpr> build();

    private:
        ArrayType& mType;
        MappingT mValues;
        ExprRef<LiteralExpr> mElse;
    };
private:
    ArrayLiteralExpr(ArrayType& type, MappingT mapping, ExprRef<LiteralExpr> elze)
        : LiteralExpr(type), mMap(std::move(mapping)), mElse(std::move(elze))
    {}

public:
    void print(llvm::raw_ostream& os) const override;

    static ExprRef<ArrayLiteralExpr> Get(
        ArrayType& type,
        const MappingT& value,
        const ExprRef<LiteralExpr>& elze = nullptr
    );

    ExprRef<LiteralExpr> operator[](const ExprRef<LiteralExpr>& key) const;

    const MappingT& getMap() const { return mMap; }

    bool hasDefault() const { return mElse != nullptr; }
    ExprRef<LiteralExpr> getDefault() const { return mElse; }

    ArrayType& getType() const {
        return llvm::cast<ArrayType>(mType);
    }

    static bool classof(const Expr* expr) {
        return expr->getKind() == Literal && expr->getType().isArrayType();
    }

    static bool classof(const Expr& expr) {
        return expr.getKind() == Literal && expr.getType().isArrayType();
    }
private:
    MappingT mMap;
    ExprRef<LiteralExpr> mElse;
};

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
