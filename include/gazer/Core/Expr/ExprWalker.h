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
/// \tparam DerivedT A Curiously Recurring Template Pattern (CRTP) parameter of
///     the derived class.
/// \tparam ReturnT The visit result. Must be a default-constructible and
///     copy-constructible.
/// \tparam SlabSize Slab size of the underlying stack allocator.
template<class DerivedT, class ReturnT, size_t SlabSize = 4096>
class ExprWalker
{
    static_assert(std::is_default_constructible_v<ReturnT>,
        "ExprWalker return type must be default-constructible!");

    size_t tmp = 0;

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

public:
    ReturnT walk(const ExprPtr& expr)
    {
        static_assert(
            std::is_base_of_v<ExprWalker, DerivedT>,
            "The derived type must be passed to the ExprWalker!"
        );

        assert(mTop == nullptr);
        mTop = createFrame(expr, 0, nullptr);

        while (mTop != nullptr) {
            Frame* current = mTop;
            ReturnT ret;
            bool shouldSkip = static_cast<DerivedT*>(this)->shouldSkip(current->mExpr, &ret);
            if (current->isFinished() || shouldSkip) {
                if (!shouldSkip) {
                    ret = this->doVisit(current->mExpr);
                    static_cast<DerivedT*>(this)->handleResult(current->mExpr, ret);
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
                return std::move(ret);
            }

            auto nn = llvm::cast<NonNullaryExpr>(current->mExpr);
            size_t i = current->mState;

            auto frame = createFrame(nn->getOperand(i), i, current);
            mTop = frame;
            current->mState++;
        }

        llvm_unreachable("Invalid walker state!");
    }

protected:
    /// Returns the operand of index \p i in the topmost frame.
    [[nodiscard]] ReturnT getOperand(size_t i) const
    {
        assert(mTop != nullptr);
        assert(i < mTop->mVisitedOps.size());
        return mTop->mVisitedOps[i];
    }

public:
    /// If this function returns true, the walker will not visit \p expr
    /// and will use the value contained in \p ret.
    bool shouldSkip(const ExprPtr& expr, ReturnT* ret) { return false; }

    /// This function is called by the walker if an actual visit took place
    /// for \p expr. The visit result is contained in \p ret.
    void handleResult(const ExprPtr& expr, ReturnT& ret) {}

    ReturnT doVisit(const ExprPtr& expr)
    {
        #define GAZER_EXPR_KIND(KIND)                                       \
            case Expr::KIND:                                                \
                return static_cast<DerivedT*>(this)->visit##KIND(           \
                    llvm::cast<KIND##Expr>(expr)                            \
                );                                                          \

        switch (expr->getKind()) {
            #include "gazer/Core/Expr/ExprKind.def"
        }

        llvm_unreachable("Unknown expression kind!");

        #undef GAZER_EXPR_KIND
    }

    void visitExpr(const ExprPtr& expr) {}

    ReturnT visitNonNullary(const ExprRef<NonNullaryExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitExpr(expr);
    }

    // Nullary
    ReturnT visitUndef(const ExprRef<UndefExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitExpr(expr);
    }

    ReturnT visitLiteral(const ExprRef<LiteralExpr>& expr)
    {
        // Disambiguate here for each literal type
        #define EXPR_LITERAL_CASE(TYPE)                                         \
            case Type::TYPE##TypeID:                                            \
                return static_cast<DerivedT*>(this)                             \
                    ->visit##TYPE##Literal(expr_cast<TYPE##LiteralExpr>(expr)); \

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

    ReturnT visitVarRef(const ExprRef<VarRefExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitExpr(expr);
    }

    // Literals
    ReturnT visitBoolLiteral(const ExprRef<BoolLiteralExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitExpr(expr);
    }
    ReturnT visitIntLiteral(const ExprRef<IntLiteralExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitExpr(expr);
    }
    ReturnT visitRealLiteral(const ExprRef<RealLiteralExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitExpr(expr);
    }
    ReturnT visitBvLiteral(const ExprRef<BvLiteralExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitExpr(expr);
    }
    ReturnT visitFloatLiteral(const ExprRef<FloatLiteralExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitExpr(expr);
    }
    ReturnT visitArrayLiteral(const ExprRef<ArrayLiteralExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitExpr(expr);
    }

    // Unary
    ReturnT visitNot(const ExprRef<NotExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitZExt(const ExprRef<ZExtExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitSExt(const ExprRef<SExtExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitExtract(const ExprRef<ExtractExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    // Binary
    ReturnT visitAdd(const ExprRef<AddExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitSub(const ExprRef<SubExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitMul(const ExprRef<MulExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitDiv(const ExprRef<DivExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitMod(const ExprRef<ModExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitRem(const ExprRef<RemExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    ReturnT visitBvSDiv(const ExprRef<BvSDivExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvUDiv(const ExprRef<BvUDivExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvSRem(const ExprRef<BvSRemExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvURem(const ExprRef<BvURemExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    ReturnT visitShl(const ExprRef<ShlExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitLShr(const ExprRef<LShrExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitAShr(const ExprRef<AShrExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvAnd(const ExprRef<BvAndExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvOr(const ExprRef<BvOrExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvXor(const ExprRef<BvXorExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvConcat(const ExprRef<BvConcatExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    // Logic
    ReturnT visitAnd(const ExprRef<AndExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitOr(const ExprRef<OrExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitXor(const ExprRef<XorExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitImply(const ExprRef<ImplyExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    // Compare
    ReturnT visitEq(const ExprRef<EqExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitNotEq(const ExprRef<NotEqExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    ReturnT visitLt(const ExprRef<LtExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitLtEq(const ExprRef<LtEqExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitGt(const ExprRef<GtExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitGtEq(const ExprRef<GtEqExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    
    ReturnT visitBvSLt(const ExprRef<BvSLtExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvSLtEq(const ExprRef<BvSLtEqExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvSGt(const ExprRef<BvSGtExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvSGtEq(const ExprRef<BvSGtEqExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    ReturnT visitBvULt(const ExprRef<BvULtExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvULtEq(const ExprRef<BvULtEqExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvUGt(const ExprRef<BvUGtExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitBvUGtEq(const ExprRef<BvUGtEqExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    // Floating-point queries
    ReturnT visitFIsNan(const ExprRef<FIsNanExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitFIsInf(const ExprRef<FIsInfExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    // Floating-point casts
    ReturnT visitFCast(const ExprRef<FCastExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    ReturnT visitSignedToFp(const ExprRef<SignedToFpExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitUnsignedToFp(const ExprRef<UnsignedToFpExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitFpToSigned(const ExprRef<FpToSignedExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitFpToUnsigned(const ExprRef<FpToUnsignedExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    // Floating-point arithmetic
    ReturnT visitFAdd(const ExprRef<FAddExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitFSub(const ExprRef<FSubExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitFMul(const ExprRef<FMulExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitFDiv(const ExprRef<FDivExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    // Floating-point compare
    ReturnT visitFEq(const ExprRef<FEqExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitFGt(const ExprRef<FGtExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitFGtEq(const ExprRef<FGtEqExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitFLt(const ExprRef<FLtExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
    ReturnT visitFLtEq(const ExprRef<FLtEqExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    // Ternary
    ReturnT visitSelect(const ExprRef<SelectExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    // Arrays
    ReturnT visitArrayRead(const ExprRef<ArrayReadExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    ReturnT visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    ReturnT visitTupleSelect(const ExprRef<TupleSelectExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }

    ReturnT visitTupleConstruct(const ExprRef<TupleConstructExpr>& expr) {
        return static_cast<DerivedT*>(this)->visitNonNullary(expr);
    }
};

} // end namespace gazer

#endif
