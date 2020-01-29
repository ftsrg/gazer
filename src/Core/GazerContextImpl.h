//==-------------------------------------------------------------*- C++ -*--==//
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
#ifndef GAZER_SRC_GAZERCONTEXTIMPL_H
#define GAZER_SRC_GAZERCONTEXTIMPL_H

#include "gazer/Core/Type.h"
#include "gazer/Core/Decl.h"
#include "gazer/Core/GazerContext.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Support/DenseMapKeyInfo.h"
#include "gazer/Support/Debug.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/Hashing.h>

#include <llvm/Support/raw_ostream.h>

#include <boost/container_hash/hash.hpp>

#include <unordered_set>
#include <unordered_map>

namespace llvm {
    template<class IntTy>
    llvm::hash_code hash_value(const boost::rational<IntTy>& arg) {
        return llvm::hash_value(std::make_pair(arg.numerator(), arg.denominator()));
    }
}

namespace gazer
{

/// Helper struct for acquiring the ExprKind enum value for a given type.
template<class ExprTy> struct ExprTypeToExprKind {};

#define GAZER_EXPR_KIND(KIND)                                  \
template<> struct ExprTypeToExprKind<KIND##Expr> {             \
    static constexpr Expr::ExprKind Kind = Expr::KIND;         \
};                                                             \

#include "gazer/Core/Expr/ExprKind.def"

#undef GAZER_EXPR_KIND

/// Returns a unique hash constant for a given expression kind.
/// Defined in src/Core/Expr.cpp.
std::size_t expr_kind_prime(Expr::ExprKind kind);

template<class InputIterator, class... SubclassData>
inline std::size_t expr_ops_hash(Expr::ExprKind kind, InputIterator begin, InputIterator end, SubclassData... subclassData)
{
    return llvm::hash_combine(
        subclassData..., expr_kind_prime(kind), llvm::hash_combine_range(begin, end)
    );
}

//-------------------------- Expression utilities ---------------------------//
// These templates contain functionality which is used to compare expressions
// without having access to an expression instance.

template<class ExprTy, class T = void> struct expr_hasher;

template<class ExprTy>
struct expr_hasher<ExprTy, std::enable_if_t<
    std::is_base_of<NonNullaryExpr, ExprTy>::value
    && !(std::is_base_of<detail::FpExprWithRoundingMode, ExprTy>::value)
>> {
    template<class InputIterator>
    static std::size_t hash_value(Expr::ExprKind kind, Type& type, InputIterator begin, InputIterator end) {
        return expr_ops_hash(kind, begin, end);
    }

    template<class InputIterator>
    static bool equals(
        const Expr* other,
        Expr::ExprKind kind, Type& type, InputIterator begin, InputIterator end
    ) {
        if (other->getKind() != kind || other->getType() != type) {
            return false;
        }

        auto nn = llvm::cast<NonNullaryExpr>(other);
        return std::equal(begin, end, nn->op_begin(), nn->op_end());
    }
};

template<class ExprTy>
const ExprTy* literal_equals(const Expr* expr, Type& type)
{
    if (expr->getKind() != Expr::Literal) {
        return nullptr;
    }

    if (expr->getType() != type) {
        return nullptr;
    }

    return llvm::cast<ExprTy>(expr);
};

// Expression hashing template for most literals.
template<class ExprTy>
struct expr_hasher<ExprTy, std::enable_if_t<std::is_base_of_v<LiteralExpr, ExprTy>>>
{
    using ValT = decltype(std::declval<ExprTy>().getValue());

    static std::size_t hash_value(Type& type, ValT value) {
        return llvm::hash_value(value);
    }

    static bool equals(const Expr* other, Type& type, ValT value) {
        if (auto i = literal_equals<ExprTy>(other, type)) {
            return i->getValue() == value;
        }

        return false;
    }
};

// Specialization for float literals, as operator==() cannot be used with APFloats.
template<> struct expr_hasher<FloatLiteralExpr>
{
    static std::size_t hash_value(Type& type, llvm::APFloat value) {
        return llvm::hash_value(value);
    }

    static bool equals(const Expr* other, Type& type, llvm::APFloat value) {
        if (auto bv = literal_equals<FloatLiteralExpr>(other, type)) {
            return bv->getValue().bitwiseIsEqual(value);
        }

        return false;
    }
};

// Specialization for array literals, using container hash
template<> struct expr_hasher<ArrayLiteralExpr>
{
    static std::size_t hash_value(
        Type& type,
        const ArrayLiteralExpr::MappingT& values,
        const ExprRef<LiteralExpr>& elze
    ) {
        std::size_t hash = 0;
        boost::hash_combine(hash, values);
        boost::hash_combine(hash, elze.get());

        return hash;
    }

    static bool equals(
        const Expr* other,
        Type& type,
        const ArrayLiteralExpr::MappingT& value,
        const ExprRef<LiteralExpr>& elze
    ) {
        if (auto arr = literal_equals<ArrayLiteralExpr>(other, type)) {
            return arr->getMap() == value && arr->getDefault() == elze;
        }

        return false;
    }
};

template<> struct expr_hasher<VarRefExpr> {
    static std::size_t hash_value(Variable* variable) {
        return llvm::hash_value(variable->getName());
    }

    static bool equals(const Expr* other, Variable* variable) {
        if (auto expr = llvm::dyn_cast<VarRefExpr>(other)) {
            return &expr->getVariable() == variable;
        }

        return false;
    }
};

template<class ExprTy>
struct expr_hasher<ExprTy, std::enable_if_t<std::is_base_of_v<detail::FpExprWithRoundingMode, ExprTy>>>
{
    template<class InputIterator>
    static std::size_t hash_value(Expr::ExprKind kind, Type& type, InputIterator begin, InputIterator end, llvm::APFloat::roundingMode rm) {
        return llvm::hash_combine(rm, expr_ops_hash(kind, begin, end));
    }

    template<class InputIterator>
    static bool equals(
        const Expr* other,
        Expr::ExprKind kind, Type& type,
        InputIterator begin, InputIterator end,
        llvm::APFloat::roundingMode rm
    ) {
        if (!expr_hasher<NonNullaryExpr>::equals(other, kind, type, begin, end)) {
            return false;
        }

        // If the previous equals call returned true,
        // the expression must be of the right type.
        auto fp = llvm::cast<ExprTy>(other);
        return fp->getRoundingMode() == rm;
    }
};

template<> struct expr_hasher<ExtractExpr>
{
    template<class InputIterator>
    static std::size_t hash_value(Expr::ExprKind kind, Type& type, InputIterator begin, InputIterator end, unsigned offset, unsigned width) {
        return llvm::hash_combine(
            offset, width,
            expr_ops_hash(Expr::Extract, begin, end)
        );
    }

    template<class InputIterator>
    static bool equals(
        const Expr* other, Expr::ExprKind kind, Type& type,
        InputIterator begin, InputIterator end, unsigned offset, unsigned width)
    {
        if (!expr_hasher<NonNullaryExpr>::equals(other, kind, type, begin, end)) {
            return false;
        }

        // If the previous equals call returned true,
        // the expression must be an ExtractExpr.
        auto extract = llvm::cast<ExtractExpr>(other);
        return extract->getOffset() == offset && extract->getWidth() == width;
    }
};

template<> struct expr_hasher<UndefExpr> {
    static std::size_t hash_value(Type& type) {
        return llvm::hash_combine(496549u, &type);
    }

    static bool equals(const Expr* other, Type& type) {
        return other->getKind() == Expr::Undef && other->getType() == type;
    }
};

template<> struct expr_hasher<TupleSelectExpr>
{
    template<class InputIterator>
    static std::size_t hash_value(
        Expr::ExprKind kind, Type& type,
        InputIterator begin, InputIterator end, unsigned index
    ) {
        return llvm::hash_combine(
            index,
            expr_ops_hash(Expr::TupleSelect, begin, end)
        );
    }

    template<class InputIterator>
    static bool equals(
        const Expr* other, Expr::ExprKind kind, Type& type,
        InputIterator begin, InputIterator end, unsigned index
    ) {
        if (!expr_hasher<NonNullaryExpr>::equals(other, kind, type, begin, end)) {
            return false;
        }

        auto tupSel = llvm::cast<TupleSelectExpr>(other);
        return tupSel->getIndex() == index;
    }
};

//--------------------------- Expression storage ----------------------------//

/// \brief Internal hashed set storage for all non-nullary expressions
/// created by a given context.
///
/// Construction is done by calling the (private) constructors of the
/// befriended expression classes.
class ExprStorage
{
    static constexpr size_t DefaultBucketCount = 64;
    using NodeT = Expr;

    struct Bucket
    {
        Bucket()
            : Ptr(nullptr)
        {}

        Bucket(const Bucket&) = delete;

        Expr* Ptr;
    };

public:
    ExprStorage()
        : mBucketCount(DefaultBucketCount)
    {
        mStorage = new Bucket[mBucketCount];
    }


    ~ExprStorage();

    template<
        class ExprTy,
        class = std::enable_if<std::is_base_of<NonNullaryExpr, ExprTy>::value>,
        class... SubclassData
    > ExprRef<ExprTy> create(Type &type, std::initializer_list<ExprPtr> init, SubclassData&&... data)
    {
        return createRange<ExprTy>(type, init.begin(), init.end(), std::forward<SubclassData>(data)...);
    }

    template<
        class ExprTy,
        class = std::enable_if<std::is_base_of<NonNullaryExpr, ExprTy>::value>,
        Expr::ExprKind Kind = ExprTypeToExprKind<ExprTy>::Kind,
        class InputIterator,
        class... SubclassData
    > ExprRef<ExprTy> createRange(
        Type& type,
        InputIterator op_begin, InputIterator op_end,
        SubclassData&&... subclassData
    ) {
        return createIfNotExists<ExprTy>(Kind, type, op_begin, op_end, std::forward<SubclassData>(subclassData)...);
    }

    template<
        class ExprTy,
        class = std::enable_if<std::is_base_of<LiteralExpr, ExprTy>::value>,
        class... ConstructorArgs
    > ExprRef<ExprTy> create(ConstructorArgs&&... args) {
        return createIfNotExists<ExprTy>(std::forward<ConstructorArgs>(args)...);
    }

    void destroy(Expr* expr);

    void rehashTable(size_t newSize);

    size_t size() const { return mEntryCount; }

private:
    template<class ExprTy, class... ConstructorArgs>
    ExprRef<ExprTy> createIfNotExists(ConstructorArgs&&... args)
    {
        auto hash = expr_hasher<ExprTy>::hash_value(args...);
        Bucket* bucket = &getBucketForHash(hash);

        Expr* current = bucket->Ptr;
        while (current != nullptr) {
            if (expr_hasher<ExprTy>::equals(current, args...)) {
                return ExprRef<ExprTy>(llvm::cast<ExprTy>(current));
            }

            current = current->mNextPtr;
        }

        if (needsRehash(mEntryCount + 1)) {
            this->rehashTable(mBucketCount * 2);

            // Rehashing invalidated the previously calculated bucket,
            // so we need to get it again.
            bucket = &getBucketForHash(hash);
        }

        auto expr = new ExprTy(args...);
        expr->mHashCode = hash;

        GAZER_DEBUG(
            llvm::errs()
                << "[ExprStorage] Created new "
                << Expr::getKindName(expr->getKind())
                << " address " << expr << "\n"
        );

        ++mEntryCount;

        expr->mNextPtr = bucket->Ptr;
        bucket->Ptr = expr;

        return ExprRef<ExprTy>(expr);
    };

    Bucket& getBucketForHash(size_t hash) const {
        return mStorage[hash % mBucketCount];
    }

    bool needsRehash(size_t entries) const {
        return entries * 4 >= mBucketCount * 3;
    }

    void removeFromList(Expr* expr);

private:
    Bucket* mStorage;
    size_t  mBucketCount;
    size_t  mEntryCount = 0;
};

class GazerContextImpl
{
    friend class GazerContext;
    explicit GazerContextImpl(GazerContext& ctx);

public:
    ~GazerContextImpl();

public:
    //---------------------- Types ----------------------//
    BoolType BoolTy;
    IntType IntTy;
    RealType RealTy;
    BvType Bv1Ty, Bv8Ty, Bv16Ty, Bv32Ty, Bv64Ty;
    std::unordered_map<unsigned, std::unique_ptr<BvType>> BvTypes;
    FloatType FpHalfTy, FpSingleTy, FpDoubleTy, FpQuadTy;
    std::unordered_map<
        std::vector<Type*>,
        std::unique_ptr<ArrayType>,
        boost::hash<std::vector<Type*>>
    > ArrayTypes;
    std::unordered_map<
        std::vector<Type*>,
        std::unique_ptr<TupleType>,
        boost::hash<std::vector<Type*>>
    > TupleTypes;
    std::unordered_map<
        std::pair<Type*, std::vector<Type*>>,
        std::unique_ptr<FunctionType>,
        boost::hash<std::pair<Type*, std::vector<Type*>>>
    > FunctionTypes;

    //------------------- Expressions -------------------//
    ExprStorage Exprs;
    ExprRef<BoolLiteralExpr> TrueLit, FalseLit;
    llvm::StringMap<std::unique_ptr<Variable>> VariableTable;

private:
};

} // end namespace gazer

#endif