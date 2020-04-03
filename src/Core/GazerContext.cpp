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
#include "GazerContextImpl.h"

#include <llvm/Support/Allocator.h>
#include <llvm/Support/MathExtras.h>
#include <llvm/Support/Debug.h>

// Disable exception handling in boost.
#ifndef BOOST_NO_EXCEPTIONS
    #error "gazer must be compiled with -fno-exceptions and BOOST_NO_EXCEPTIONS"
#else
namespace boost
{
    void throw_exception(std::exception const &e)
    {
        #ifndef NDEBUG
        llvm::errs() << "Boost internal error: " << e.what();
        #endif
        std::terminate();
    }
} // end namespace boost
#endif

// TODO: Move this.
#ifdef GAZER_ENABLE_DEBUG
bool ::gazer::IsDebugEnabled = true;
#else
bool ::gazer::IsDebugEnabled = false;
#endif

#ifndef NDEBUG
#define GAZER_DEBUG_ASSERT(X) assert(X)
#else
#define GAZER_DEBUG_ASSERT(X)
#endif

#define DEBUG_TYPE "GazerContext"

using namespace gazer;

GazerContext::GazerContext()
    : pImpl(new GazerContextImpl(*this))
{}

GazerContext::~GazerContext() = default;

//-------------------------------- Variables --------------------------------//

Variable* GazerContext::createVariable(const std::string& name, Type &type)
{
    LLVM_DEBUG(llvm::dbgs() << "Adding variable with name " << name << " and type " << type << "\n");
    GAZER_DEBUG_ASSERT(pImpl->VariableTable.count(name) == 0);
    auto ptr = new Variable(name, type);
    pImpl->VariableTable[name] = std::unique_ptr<Variable>(ptr);

    GAZER_DEBUG(llvm::errs()
        << "[GazerContext] Adding variable with name: '"
        << name
        << " address "
        << ptr
        << "'\n")

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

void GazerContext::removeVariable(Variable* variable)
{
    auto result = pImpl->VariableTable.find(variable->getName());
    assert(result != pImpl->VariableTable.end() && "Attempting to delete a non-existant variable!");

    pImpl->VariableTable.erase(result);
}

//------------------------------- Expressions -------------------------------//

void ExprStorage::removeFromList(Expr* expr)
{
    Bucket& bucket = getBucketForHash(expr->getHashCode());

    // If this was the first element in the bucket
    if (bucket.Ptr == expr) {
        bucket.Ptr = expr->mNextPtr;
    } else {
        Expr* prev = nullptr;
        Expr* current = bucket.Ptr;

        assert(current != expr && "current must not be the first element in the bucket!");

        while (current != expr) {
            prev = current;
            current = current->mNextPtr;
        }

        prev->mNextPtr = current->mNextPtr;
    }

    expr->mNextPtr = nullptr;
}

void ExprStorage::destroy(Expr *expr)
{
    GAZER_DEBUG(llvm::errs()
        << "[ExprStorage] Removing "
        << Expr::getKindName(expr->getKind())
        << " address " << expr
        << " content " << *expr
        << "\n"
    )

    this->removeFromList(expr);
    --mEntryCount;

    if (!llvm::isa<NonNullaryExpr>(expr)) {
        delete expr;
        return;
    }

    // For really large and deep expression trees the chain of
    // delete -> destroy -> delete calls may cause a stack oveflow error.
    // Here we overcome this issue by traversing the expression and all its
    // operands, pushing all would-be deleted expressions onto a list and
    // deleting them afterwards.
    // The list is built by reusing the available expression nodes
    // (through the mNextPtr's) to form a singly linked list,
    // thus no heap allocation is needed.
    auto tail = llvm::cast<NonNullaryExpr>(expr);
    auto last = tail;

    while (last != nullptr) {
        for (size_t i = 0; i < last->mOperands.size(); ++i) {
            Expr* child = last->getOperand(i).get();
            if (child->mRefCount == 1) {
                // If this is the only pointer pointing at the expression, remove it.
                this->removeFromList(child);
                --mEntryCount;

                GAZER_DEBUG(llvm::errs()
                    << "[ExprStorage] Adding for deletion "
                    << Expr::getKindName(child->getKind())
                    << " address " << child
                    << "\n"
                )

                if (auto nn = llvm::dyn_cast<NonNullaryExpr>(child)) {
                    // Append non-nullaries to the end of the list.
                    tail->mNextPtr = nn;
                    tail = nn;
                } else {
                    // If it is a leaf node, just delete it.
                    delete child;
                }
            } else {
                last->mOperands[i]->mRefCount--;
            }

            last->mOperands[i].detach();
        }

        last = llvm::cast_or_null<NonNullaryExpr>(last->mNextPtr);
    }

    Expr* current = expr;
    while (current != nullptr) {
        GAZER_DEBUG(llvm::errs()
            << "[ExprStorage] Deleted "
            << " address " << current
            << "\n"
        )
        Expr* next = current->mNextPtr;
        delete current;
        current = next;
    }
}

void ExprStorage::rehashTable(size_t newSize)
{
    GAZER_DEBUG(llvm::errs() << "[ExprStorage] Extending table " << newSize << "\n")
    auto newStorage = new Bucket[newSize];

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
            GAZER_DEBUG(llvm::errs()
                << "[ExprStorage] Leaking expression! "
                << current << "\n")
            Expr* next = current->mNextPtr;
            delete current;
            current = next;
        }
    }

    delete[] mStorage;
}

void GazerContext::dumpStats(llvm::raw_ostream& os) const
{
    os << "Number of expressions: " << pImpl->Exprs.size() << "\n";
    os << "Number of variables: " << pImpl->VariableTable.size() << "\n";
}

//-------------------------------- Resources --------------------------------//

GazerContextImpl::GazerContextImpl(GazerContext& ctx)
    :
    // Types
    BoolTy(ctx), IntTy(ctx), RealTy(ctx),
    Bv1Ty(ctx, 1), Bv8Ty(ctx, 8), Bv16Ty(ctx, 16), Bv32Ty(ctx, 32), Bv64Ty(ctx, 64),
    FpHalfTy(ctx, FloatType::Half), FpSingleTy(ctx, FloatType::Single),
    FpDoubleTy(ctx, FloatType::Double), FpQuadTy(ctx, FloatType::Quad),
    // Expressions
    TrueLit(new BoolLiteralExpr(BoolTy, true)),
    FalseLit(new BoolLiteralExpr(BoolTy, false))
{
    TrueLit->mHashCode = llvm::hash_value(TrueLit.get());
    FalseLit->mHashCode = llvm::hash_value(FalseLit.get());
}

GazerContextImpl::~GazerContextImpl() = default;
