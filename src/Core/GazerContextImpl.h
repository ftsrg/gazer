#ifndef GAZER_SRC_GAZERCONTEXTIMPL_H
#define GAZER_SRC_GAZERCONTEXTIMPL_H

#include "gazer/Core/GazerContext.h"
#include "gazer/Core/Type.h"
#include "gazer/Core/Variable.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Support/DenseMapKeyInfo.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/Hashing.h>

#include <llvm/Support/raw_ostream.h>

#include <boost/container_hash/hash.hpp>

#include <unordered_set>
#include <unordered_map>

namespace gazer
{

/// Helper struct for acquiring the ExprKind enum value for a given type.
template<class ExprTy> struct ExprTypeToExprKind {};

#define GAZER_EXPR_KIND(KIND)                                  \
template<> struct ExprTypeToExprKind<KIND##Expr> {             \
    static constexpr Expr::ExprKind Kind = Expr::KIND;         \
};                                                             \

#include "gazer/Core/Expr/ExprKind.inc"

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

template<class ExprTy, class = std::enable_if<std::is_base_of<NonNullaryExpr, ExprTy>::value>>
struct expr_hasher {
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

        return std::equal(begin, end, llvm::cast<NonNullaryExpr>(other)->op_begin());
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

template<> struct expr_hasher<BvLiteralExpr> {
    static std::size_t hash_value(Type& type, llvm::APInt value) {
        return llvm::hash_value(value);
    }

    static bool equals(const Expr* other, Type& type, llvm::APInt value) {
        if (auto bv = literal_equals<BvLiteralExpr>(other, type)) {
            return bv->getValue() == value;
        }

        return false;
    }
};

template<> struct expr_hasher<FloatLiteralExpr> {
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

template<Expr::ExprKind Kind> struct expr_hasher<FpArithmeticExpr<Kind>> {
    template<class InputIterator>
    static std::size_t hash_value(Expr::ExprKind kind, Type& type, InputIterator begin, InputIterator end, llvm::APFloat::roundingMode rm) {
        return llvm::hash_combine(rm, expr_ops_hash(Kind, begin, end));
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
        auto fp = llvm::cast<FpArithmeticExpr<Kind>>(other);
        return fp->getRoundingMode() == rm;
    }
};

template<> struct expr_hasher<ExtractExpr> {
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
        InputIterator begin, InputIterator end, unsigned offset, unsigned width
    ) {
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

        Expr* Ptr;
    };

public:
    ExprStorage()
        : mBucketCount(DefaultBucketCount)
    {
        mStorage = new Bucket[mBucketCount];
    }


    ~ExprStorage();

    template<class ExprTy, class... SubclassData>
    ExprRef<ExprTy> create(Type &type, std::initializer_list<ExprPtr> init, SubclassData&&... data)
    {
        return createRange<ExprTy>(type, init.begin(), init.end(), data...);
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
        return createIfNotExists<ExprTy>(Kind, type, op_begin, op_end, subclassData...);
    }

    template<
        class ExprTy,
        class = std::enable_if<std::is_base_of<LiteralExpr, ExprTy>::value>,
        class... ConstructorArgs
    > ExprRef<ExprTy> create(ConstructorArgs&&... args) {
        return createIfNotExists<ExprTy>(args...);
    }

    void destroy(Expr* expr);
    void rehashTable(size_t newSize);

private:

    template<class ExprTy, class... ConstructorArgs>
    ExprRef<ExprTy> createIfNotExists(ConstructorArgs&&... args)
    {
        auto hash = expr_hasher<ExprTy>::hash_value(args...);
        Bucket& bucket = this->getBucketForHash(hash);

        Expr* current = bucket.Ptr;
        while (current != nullptr) {
            if (expr_hasher<ExprTy>::equals(current, args...)) {
                return ExprRef<ExprTy>(llvm::cast<ExprTy>(current));
            }

            current = current->mNextPtr;
        }

        size_t numNewEntries = mEntryCount + 1;
        if (numNewEntries * 4 >= mBucketCount * 3) {
            this->rehashTable(mBucketCount * 2);
        }

        ExprTy* expr = new ExprTy(args...);
        expr->mHashCode = hash;

        ++mEntryCount;

        expr->mNextPtr = bucket.Ptr;
        bucket.Ptr = expr;

        return ExprRef<ExprTy>(expr);
    };

    Bucket& getBucketForHash(size_t hash) {
        return mStorage[hash % mBucketCount];
    }

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
    //---------------------- Types ----------------------//
    BoolType BoolTy;
    IntType IntTy;
    BvType Bv1Ty, Bv8Ty, Bv16Ty, Bv32Ty, Bv64Ty;
    std::unordered_map<unsigned, std::unique_ptr<BvType>> BvTypes;
    FloatType FpHalfTy, FpSingleTy, FpDoubleTy, FpQuadTy;

    //-------------------- Variables --------------------//
    // This map contains all variables created by this context, with FQNs as the map key.
    llvm::StringMap<std::unique_ptr<Variable>> VariableTable;

    //------------------- Expressions -------------------//
    ExprRef<BoolLiteralExpr> TrueLit, FalseLit;

    // This set will contain all expressions created by this context.
    ExprStorage Exprs;

    ~GazerContextImpl();
};

} // end namespace gazer

#endif