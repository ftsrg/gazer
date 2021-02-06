//==- ExprWalker.h - Expression visitor interface ---------------*- C++ -*--==//
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
#ifndef GAZER_CORE_EXPR_EXPRWALKER_H
#define GAZER_CORE_EXPR_EXPRWALKER_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include "gazer/Support/GrowingStackAllocator.h"

#include <llvm/ADT/SmallVector.h>

namespace gazer
{

/// Generic walker interface for expressions.
/// 
/// This class avoids recursion by using an explicit stack on the heap instead
/// of the normal call stack. This allows us to avoid stack overflow errors in
/// the case of large input expressions. The order of the traversal is fixed
/// to be post-order, thus all (translated/visited) operands of an expression
/// are available in the visit() method. They may be retrieved by calling the
/// getOperand(size_t) method.
/// 
/// In order to support caching, users may override the shouldSkip() and
/// handleResult() functions. The former should return true if the cache
/// was hit and set the found value. The latter should be used to insert
/// new entries into the cache.
///
/// \tparam ReturnT The visit result. Must be a default-constructible and
///     copy-constructible.
/// \tparam SlabSize Slab size of the underlying stack allocator.
template<class ReturnT, size_t SlabSize = 4096>
class ExprWalker
{
    static_assert(std::is_default_constructible_v<ReturnT>,
        "ExprWalker return type must be default-constructible!");

    struct Frame
    {
        ExprPtr mExpr;
        size_t mIndex;
        Frame* mParent = nullptr;
        size_t mState = 0;
        llvm::SmallVector<ReturnT, 2> mVisitedOps;

        Frame(ExprPtr expr, size_t index, Frame* parent)
            : mExpr(std::move(expr)),
            mIndex(index),
            mParent(parent),
            mVisitedOps(
                mExpr->isNullary() ? 0 : llvm::cast<NonNullaryExpr>(mExpr)->getNumOperands()
            )
        {}

        bool isFinished() const
        {
            return mExpr->isNullary() || llvm::cast<NonNullaryExpr>(mExpr)->getNumOperands() == mState;
        }
    };
private:
    Frame* createFrame(const ExprPtr& expr, size_t idx, Frame* parent)
    {
        size_t siz = sizeof(Frame);
        void* ptr = mAllocator.Allocate(siz, alignof(Frame));

        return new (ptr) Frame(expr, idx, parent);
    }

    void destroyFrame(Frame* frame)
    {
        frame->~Frame();
        mAllocator.Deallocate(frame, sizeof(Frame));
    }

private:
    Frame* mTop;
    GrowingStackAllocator<llvm::MallocAllocator, SlabSize> mAllocator;

public:
    ExprWalker()
        : mTop(nullptr)
    {
        mAllocator.Init();
    }

    ExprWalker(const ExprWalker&) = delete;
    ExprWalker& operator=(ExprWalker&) = delete;

    ~ExprWalker() = default;

protected:
    /// If this function returns true, the walker will not visit \p expr
    /// and will use the value contained in \p ret.
    virtual bool shouldSkip(const ExprPtr& expr, ReturnT* ret) { return false; }

    /// This function is called by the walker if an actual visit took place
    /// for \p expr. The visit result is contained in \p ret.
    virtual void handleResult(const ExprPtr& expr, ReturnT& ret) {}

    ReturnT walk(const ExprPtr& expr)
    {
        assert(mTop == nullptr);
        mTop = createFrame(expr, 0, nullptr);

        while (mTop != nullptr) {
            Frame* current = mTop;
            ReturnT ret;
            bool shouldSkip = this->shouldSkip(current->mExpr, &ret);
            if (current->isFinished() || shouldSkip) {
                if (!shouldSkip) {
                    ret = this->doVisit(current->mExpr);
                    this->handleResult(current->mExpr, ret);
                }
                Frame* parent = current->mParent;
                size_t idx = current->mIndex;
                this->destroyFrame(current);

                if (LLVM_LIKELY(parent != nullptr)) {
                    parent->mVisitedOps[idx] = std::move(ret);
                    mTop = parent;
                    continue;
                }
                
                mTop = nullptr;
                return ret;
            }

            auto nn = llvm::cast<NonNullaryExpr>(current->mExpr);
            size_t i = current->mState;

            auto frame = createFrame(nn->getOperand(i), i, current);
            mTop = frame;
            current->mState++;
        }

        llvm_unreachable("Invalid walker state!");
    }

    /// Returns the operand of index \p i in the topmost frame.
    [[nodiscard]] ReturnT getOperand(size_t i) const
    {
        assert(mTop != nullptr);
        assert(i < mTop->mVisitedOps.size());
        return mTop->mVisitedOps[i];
    }

    ReturnT doVisit(const ExprPtr& expr)
    {
        #define GAZER_EXPR_KIND(KIND)                                       \
            case Expr::KIND:                                                \
                return this->visit##KIND(                                   \
                    llvm::cast<KIND##Expr>(expr)                            \
                );                                                          \

        switch (expr->getKind()) {
            #include "gazer/Core/Expr/ExprKind.def"
        }

        llvm_unreachable("Unknown expression kind!");

        #undef GAZER_EXPR_KIND
    }

    // Overridable visitation methods
    //==--------------------------------------------------------------------==//

    virtual ReturnT visitExpr(const ExprPtr& expr) = 0;

    virtual ReturnT visitNonNullary(const ExprRef<NonNullaryExpr>& expr) {
        return this->visitExpr(expr);
    }

    // Nullary
    virtual ReturnT visitUndef(const ExprRef<UndefExpr>& expr) {
        return this->visitExpr(expr);
    }

    virtual ReturnT visitLiteral(const ExprRef<LiteralExpr>& expr)
    {
        // Disambiguate here for each literal type
        #define EXPR_LITERAL_CASE(TYPE)                                                 \
            case Type::TYPE##TypeID:                                                    \
                return this->visit##TYPE##Literal(expr_cast<TYPE##LiteralExpr>(expr));  \

        switch (expr->getType().getTypeID()) {
            EXPR_LITERAL_CASE(Bool)
            EXPR_LITERAL_CASE(Int)
            EXPR_LITERAL_CASE(Real)
            EXPR_LITERAL_CASE(Bv)
            EXPR_LITERAL_CASE(Float)
            EXPR_LITERAL_CASE(Array)
            case Type::TupleTypeID:
            case Type::FunctionTypeID:
                llvm_unreachable("Invalid literal expression type!");
                break;
        }

        #undef EXPR_LITERAL_CASE

        llvm_unreachable("Unknown literal expression kind!");
    }

    virtual ReturnT visitVarRef(const ExprRef<VarRefExpr>& expr) {
        return this->visitExpr(expr);
    }

    // Literals
    virtual ReturnT visitBoolLiteral(const ExprRef<BoolLiteralExpr>& expr) {
        return this->visitExpr(expr);
    }
    virtual ReturnT visitIntLiteral(const ExprRef<IntLiteralExpr>& expr) {
        return this->visitExpr(expr);
    }
    virtual ReturnT visitRealLiteral(const ExprRef<RealLiteralExpr>& expr) {
        return this->visitExpr(expr);
    }
    virtual ReturnT visitBvLiteral(const ExprRef<BvLiteralExpr>& expr) {
        return this->visitExpr(expr);
    }
    virtual ReturnT visitFloatLiteral(const ExprRef<FloatLiteralExpr>& expr) {
        return this->visitExpr(expr);
    }
    virtual ReturnT visitArrayLiteral(const ExprRef<ArrayLiteralExpr>& expr) {
        return this->visitExpr(expr);
    }

    // Include non-nullaries
    #define GAZER_EXPR_KIND(TYPE) // Empty
    #define GAZER_NON_NULLARY_EXPR_KIND(TYPE)                                           \
        virtual ReturnT visit##TYPE(const ExprRef<TYPE##Expr>& expr) {                  \
            return this->visitNonNullary(expr);                                         \
        }

    #include "gazer/Core/Expr/ExprKind.def"

    #undef GAZER_NON_NULLARY_EXPR_KIND
    #undef GAZER_EXPR_KIND
};

} // end namespace gazer

#endif
