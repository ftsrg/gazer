
#include "GazerContextImpl.h"

#include <llvm/Support/Allocator.h>
#include <llvm/Support/MathExtras.h>

#include <unordered_set>

// PURGE_ON_REHASH: If true,
#ifndef GAZER_CONFIG_PURGE_ON_REHASH
#define GAZER_CONFIG_PURGE_ON_REHASH true
#endif


using namespace gazer;

GazerContext::GazerContext()
    : pImpl(new GazerContextImpl(*this))
{}

GazerContext::~GazerContext() {}

//-------------------------------- Variables --------------------------------//

Variable* GazerContext::createVariable(std::string name, Type &type)
{
    auto ptr = new Variable(name, type);
    pImpl->VariableTable[name] = std::unique_ptr<Variable>(ptr);

    return ptr;
}

Variable* GazerContext::getVariable(llvm::StringRef name)
{
    auto result = pImpl->VariableTable.find(name);
    if (result == pImpl->VariableTable.end()) {
        return nullptr;
    }

    return result->second.get();
}

//------------------------------- Expressions -------------------------------//

void ExprStorage::destroy(Expr *expr)
{
    Bucket& bucket = getBucketForHash(expr->getHashCode());

    // If this was the first element in the bucket
    if (bucket.Ptr == expr) {
        bucket.Ptr = expr->mNextPtr;
    } else {
        Expr* prev = nullptr;
        Expr* current = bucket.Ptr;

        while (current != expr) {
            prev = current;
            current = current->mNextPtr;
        }

        prev->mNextPtr = current->mNextPtr;
    }

    --mEntryCount;
    delete expr;
}

void ExprStorage::rehashTable(size_t newSize)
{
    Bucket* newStorage = new Bucket[newSize];

    size_t copied = 0;
    for (size_t i = 0; i < mBucketCount; ++i) {
        Bucket& oldBucket = mStorage[i];

        Expr* current = oldBucket.Ptr;
        while (current != nullptr) {
            Expr* next = current->mNextPtr;
            auto hash = current->getHashCode();

            Bucket& newBucket = newStorage[hash % newSize];
            current->mNextPtr = newBucket.Ptr;
            newBucket.Ptr = current;

            current = next;
            ++copied;
        }
    }

    delete[] mStorage;
    mBucketCount = newSize;
    mStorage = newStorage;
}

ExprStorage::~ExprStorage()
{
    // Free each expression stored in the buckets
    for (size_t i = 0; i < mBucketCount; ++i) {
        Bucket& bucket = mStorage[i];

        Expr* current = bucket.Ptr;

        while (current != nullptr) {
            Expr* next = current->mNextPtr;
            delete current;
            current = next;
        }
    }

    delete[] mStorage;
}

GazerContextImpl::GazerContextImpl(GazerContext& ctx)
    :
    // Types
    BoolTy(ctx), IntTy(ctx),
    Bv1Ty(ctx, 1), Bv8Ty(ctx, 8), Bv16Ty(ctx, 16), Bv32Ty(ctx, 32), Bv64Ty(ctx, 64),
    FpHalfTy(ctx, FloatType::Half), FpSingleTy(ctx, FloatType::Single),
    FpDoubleTy(ctx, FloatType::Double), FpQuadTy(ctx, FloatType::Quad),
    // Expressions
    TrueLit(new BoolLiteralExpr(BoolTy, true)),
    FalseLit(new BoolLiteralExpr(BoolTy, false))
{
}

GazerContextImpl::~GazerContextImpl()
{

}
