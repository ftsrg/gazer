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
#ifndef GAZER_SRC_SOLVERZ3_Z3SOLVERIMPL_H
#define GAZER_SRC_SOLVERZ3_Z3SOLVERIMPL_H

#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Core/Expr/ExprWalker.h"
#include "gazer/ADT/ScopedCache.h"

#include <llvm/Support/raw_ostream.h>
#include <z3++.h>

namespace gazer
{

// Implementation based upon Z3NodeHandle in the KLEE project:
// https://github.com/klee/klee/blob/master/lib/Solver/Z3Builder.h
template<class T>
class Z3Handle
{
public:
    Z3Handle() = default;

    Z3Handle(Z3_context context, T ast)
        : mContext(context), mNode(ast)
    {
        assert(context != nullptr);
        assert(ast != nullptr);
        Z3_inc_ref(mContext, as_ast());
    }

    Z3Handle(const Z3Handle& other)
        : mContext(other.mContext), mNode(other.mNode)
    {
        if (mContext != nullptr && mNode != nullptr) {
            Z3_inc_ref(mContext, as_ast());
        }
    }
    
    Z3Handle(Z3Handle&& other) noexcept
        : mContext(other.mContext), mNode(other.mNode)
    {
        other.mContext = nullptr;
        other.mNode = nullptr;
    }

    Z3Handle& operator=(const Z3Handle& other)
    {
        if (this != &other) {
            if (mContext == nullptr && mNode == nullptr) {
                mContext = other.mContext;
            }

            assert(mContext == other.mContext);
            // If the node is not null then the context should not be null either.
            assert(mNode == nullptr || mContext != nullptr);

            if (mContext != nullptr && mNode != nullptr) {
                Z3_dec_ref(mContext, as_ast());
            }
            mNode = other.mNode;
            if (mContext != nullptr && mNode != nullptr) {
                Z3_inc_ref(mContext, as_ast());
            }
        }

        return *this;
    }

    Z3Handle& operator=(Z3Handle&& other) noexcept
    {
        if (this != &other) {
            if (mContext != nullptr && mNode != nullptr) {
                Z3_dec_ref(mContext, as_ast());
            }

            mContext = other.mContext;
            mNode = other.mNode;
            other.mContext = nullptr;
            other.mNode = nullptr;
        }

        return *this;
    }

    bool operator==(const Z3Handle<T>& rhs) const {
        return rhs.mContext == mContext && rhs.mNode == mNode;
    }

    bool operator!=(const Z3Handle<T>& rhs) const {
        return !operator==(rhs);
    }

    /*implicit*/ operator T()
    {
        assert(mContext != nullptr);
        assert(mNode != nullptr);

        return mNode;
    }

    Z3_context getContext() const { return mContext; }
    T getNode() const { return mNode; }

    ~Z3Handle()
    {
        if (mContext != nullptr && mNode != nullptr) {
            Z3_dec_ref(mContext, as_ast());
        }
    }
private:
    // Must be specialized
    inline ::Z3_ast as_ast();

private:
    Z3_context mContext = nullptr;
    T mNode = nullptr;
};

template<> inline Z3_ast Z3Handle<Z3_sort>::as_ast() {
    return Z3_sort_to_ast(mContext, mNode);
}

template<> inline Z3_ast Z3Handle<Z3_ast>::as_ast() {
    return mNode;
}

template<> inline Z3_ast Z3Handle<Z3_func_decl>::as_ast() {
    return Z3_func_decl_to_ast(mContext, mNode);
}

} // end namespace gazer

// Provide a hash implementation for the AST handle
namespace std
{

template<class T>
struct hash<gazer::Z3Handle<T>>
{
    std::size_t operator()(const gazer::Z3Handle<T>& key) const
    {
        return llvm::hash_combine(key.getContext(), key.getNode());
    }
};

} // end namespace std

namespace gazer
{

using Z3AstHandle = Z3Handle<Z3_ast>;
using Z3CacheMapTy = ScopedCache<ExprPtr, Z3AstHandle, std::unordered_map<ExprPtr, Z3AstHandle>>;
using Z3DeclMapTy = ScopedCache<
    Variable*, Z3Handle<Z3_func_decl>, std::unordered_map<Variable*, Z3Handle<Z3_func_decl>>
>;

/// Translates expressions into Z3 nodes.
class Z3ExprTransformer : private ExprWalker<Z3AstHandle>
{
    struct TupleInfo
    {
        Z3Handle<Z3_sort> sort;
        Z3Handle<Z3_func_decl> constructor;
        std::vector<Z3Handle<Z3_func_decl>> projections;
    };

public:
    Z3ExprTransformer(
        Z3_context& context, unsigned& tmpCount,
        Z3CacheMapTy& cache, Z3DeclMapTy& decls
    )
        : mZ3Context(context), mTmpCount(tmpCount), mCache(cache), mDecls(decls)
    {}

    Z3AstHandle translate(const ExprPtr& expr)
    {
        return this->walk(expr);
    }

    /// Free up all data owned by this object.
    void clear() {
        mTupleInfo.clear();
    }

protected:
    Z3AstHandle createHandle(Z3_ast ast);

    Z3Handle<Z3_sort> typeToSort(Type& type);
    Z3Handle<Z3_sort> handleTupleType(TupleType& tupType);

    Z3Handle<Z3_func_decl> translateDecl(Variable* variable);

    Z3AstHandle translateLiteral(const ExprRef<LiteralExpr>& expr);

private:
    bool shouldSkip(const ExprPtr& expr, Z3AstHandle* ret) override;
    void handleResult(const ExprPtr& expr, Z3AstHandle& ret) override;

    Z3AstHandle visitExpr(const ExprPtr& expr) override // NOLINT(readability-convert-member-functions-to-static)
    {
        llvm::errs() << *expr << "\n";
        llvm_unreachable("Unhandled expression type in Z3ExprTransformer.");
    }


    // Use some helper macros to translate trivial cases
    #define TRANSLATE_UNARY_OP(NAME, Z3_METHOD)                                     \
    Z3AstHandle visit##NAME(const ExprRef<NAME##Expr>& expr) override {             \
        return createHandle(Z3_METHOD(mZ3Context, getOperand(0)));                  \
    }                                                                               \

    #define TRANSLATE_BINARY_OP(NAME, Z3_METHOD)                                    \
    Z3AstHandle visit##NAME(const ExprRef<NAME##Expr>& expr) override {             \
        return createHandle(Z3_METHOD(mZ3Context, getOperand(0), getOperand(1)));   \
    }                                                                               \

    #define TRANSLATE_TERNARY_OP(NAME, Z3_METHOD)                                   \
    Z3AstHandle visit##NAME(const ExprRef<NAME##Expr>& expr) override {             \
        return createHandle(Z3_METHOD(                                              \
            mZ3Context, getOperand(0), getOperand(1), getOperand(2)));              \
    }                                                                               \

    #define TRANSLATE_BINARY_FPA_RM(NAME, Z3_METHOD)                                \
    Z3AstHandle visit##NAME(const ExprRef<NAME##Expr>& expr) override {             \
        return createHandle(Z3_METHOD(mZ3Context,                                   \
            transformRoundingMode(expr->getRoundingMode()),                         \
            getOperand(0), getOperand(1)                                            \
        ));                                                                         \
    }                                                                               \

    // Nullary
    Z3AstHandle visitVarRef(const ExprRef<VarRefExpr>& expr) override;

    Z3AstHandle visitUndef(const ExprRef<UndefExpr>& expr) override
    {
        // Undef has special semantics here: as we only translate a formula only once and the result
        // is cached, should an undef reach the solver, the fresh constant created with it is cached
        // along the parent formula. This contradicts the general semantics of Undef, however, this
        // way we do not have cases where given a formula F1 containing and undef, the formula
        // And(F1, Not(F1)) is satisfiable.

        // TODO(sallaigy): Fix this behavior (maybe display an error?) if undef lifting is implemented for CFA.
        return createHandle(Z3_mk_fresh_const(mZ3Context, "", typeToSort(expr->getType())));
    }

    Z3AstHandle visitLiteral(const ExprRef<LiteralExpr>& expr) override {
        return this->translateLiteral(expr);
    }

    // Unary logic
    TRANSLATE_UNARY_OP(Not,         Z3_mk_not)

    // Arithmetic operators
    Z3AstHandle visitAdd(const ExprRef<AddExpr>& expr) override;
    Z3AstHandle visitSub(const ExprRef<SubExpr>& expr) override;
    Z3AstHandle visitMul(const ExprRef<MulExpr>& expr) override;

    TRANSLATE_BINARY_OP(Div,        Z3_mk_div)
    TRANSLATE_BINARY_OP(Mod,        Z3_mk_mod)
    TRANSLATE_BINARY_OP(Rem,        Z3_mk_rem)

    // Binary logic
    TRANSLATE_BINARY_OP(Imply,      Z3_mk_implies)

    // Multiary logic
    Z3AstHandle visitAnd(const ExprRef<AndExpr>& expr) override;
    Z3AstHandle visitOr(const ExprRef<OrExpr>& expr) override;

    // Bit-vectors
    TRANSLATE_BINARY_OP(BvSDiv,     Z3_mk_bvsdiv)
    TRANSLATE_BINARY_OP(BvUDiv,     Z3_mk_bvudiv)
    TRANSLATE_BINARY_OP(BvSRem,     Z3_mk_bvsrem)
    TRANSLATE_BINARY_OP(BvURem,     Z3_mk_bvurem)
    TRANSLATE_BINARY_OP(Shl,        Z3_mk_bvshl)
    TRANSLATE_BINARY_OP(LShr,       Z3_mk_bvlshr)
    TRANSLATE_BINARY_OP(AShr,       Z3_mk_bvashr)
    TRANSLATE_BINARY_OP(BvAnd,      Z3_mk_bvand)
    TRANSLATE_BINARY_OP(BvOr,       Z3_mk_bvor)
    TRANSLATE_BINARY_OP(BvXor,      Z3_mk_bvxor)
    TRANSLATE_BINARY_OP(BvConcat,   Z3_mk_concat)

    // Comparisons
    TRANSLATE_BINARY_OP(Eq,         Z3_mk_eq)
    TRANSLATE_BINARY_OP(Lt,         Z3_mk_lt)
    TRANSLATE_BINARY_OP(LtEq,       Z3_mk_le)
    TRANSLATE_BINARY_OP(Gt,         Z3_mk_gt)
    TRANSLATE_BINARY_OP(GtEq,       Z3_mk_ge)

    Z3AstHandle visitNotEq(const ExprRef<NotEqExpr>& expr) override;

    // Bit-vector comparisons
    TRANSLATE_BINARY_OP(BvSLt,      Z3_mk_bvslt)
    TRANSLATE_BINARY_OP(BvSLtEq,    Z3_mk_bvsle)
    TRANSLATE_BINARY_OP(BvSGt,      Z3_mk_bvsgt)
    TRANSLATE_BINARY_OP(BvSGtEq,    Z3_mk_bvsge)
    TRANSLATE_BINARY_OP(BvULt,      Z3_mk_bvult)
    TRANSLATE_BINARY_OP(BvULtEq,    Z3_mk_bvule)
    TRANSLATE_BINARY_OP(BvUGt,      Z3_mk_bvugt)
    TRANSLATE_BINARY_OP(BvUGtEq,    Z3_mk_bvuge)

    // Floating-point queries
    TRANSLATE_UNARY_OP(FIsNan,      Z3_mk_fpa_is_nan)
    TRANSLATE_UNARY_OP(FIsInf,      Z3_mk_fpa_is_infinite)

    // Floating-point compare
    TRANSLATE_BINARY_OP(FEq,        Z3_mk_fpa_eq)
    TRANSLATE_BINARY_OP(FGt,        Z3_mk_fpa_gt)
    TRANSLATE_BINARY_OP(FGtEq,      Z3_mk_fpa_geq)
    TRANSLATE_BINARY_OP(FLt,        Z3_mk_fpa_lt)
    TRANSLATE_BINARY_OP(FLtEq,      Z3_mk_fpa_leq)

    // Floating-point arithmetic
    TRANSLATE_BINARY_FPA_RM(FAdd,   Z3_mk_fpa_add)
    TRANSLATE_BINARY_FPA_RM(FMul,   Z3_mk_fpa_mul)
    TRANSLATE_BINARY_FPA_RM(FSub,   Z3_mk_fpa_sub)
    TRANSLATE_BINARY_FPA_RM(FDiv,   Z3_mk_fpa_div)

    // ITE expression
    TRANSLATE_TERNARY_OP(Select,    Z3_mk_ite)

    // Arrays
    TRANSLATE_BINARY_OP(ArrayRead,   Z3_mk_select)
    TRANSLATE_TERNARY_OP(ArrayWrite, Z3_mk_store)

    #undef TRANSLATE_UNARY_OP
    #undef TRANSLATE_BINARY_OP
    #undef TRANSLATE_TERNARY_OP
    #undef TRANSLATE_BINARY_FPA_RM

    // Bit-vector casts
    Z3AstHandle visitZExt(const ExprRef<ZExtExpr>& expr) override {
        return createHandle(Z3_mk_zero_ext(mZ3Context, expr->getWidthDiff(), getOperand(0)));
    }

    Z3AstHandle visitSExt(const ExprRef<SExtExpr>& expr) override {
        return createHandle(Z3_mk_sign_ext(mZ3Context, expr->getWidthDiff(), getOperand(0)));
    }

    Z3AstHandle visitExtract(const ExprRef<ExtractExpr>& expr) override
    {
        unsigned hi = expr->getOffset() + expr->getWidth() - 1;
        unsigned lo = expr->getOffset();

        return createHandle(Z3_mk_extract(mZ3Context, hi, lo, getOperand(0)));
    }

    // Floating-point casts
    Z3AstHandle visitFCast(const ExprRef<FCastExpr>& expr) override
    {
        return createHandle(Z3_mk_fpa_to_fp_float(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            typeToSort(expr->getType())  
        ));
    }

    Z3AstHandle visitSignedToFp(const ExprRef<SignedToFpExpr>& expr) override
    {
        return createHandle(Z3_mk_fpa_to_fp_signed(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            typeToSort(expr->getType())
        ));
    }

    Z3AstHandle visitUnsignedToFp(const ExprRef<UnsignedToFpExpr>& expr) override
    {
        return createHandle(Z3_mk_fpa_to_fp_unsigned(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            typeToSort(expr->getType())
        ));
    }

    Z3AstHandle visitFpToSigned(const ExprRef<FpToSignedExpr>& expr) override
    {
        return createHandle(Z3_mk_fpa_to_sbv(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            llvm::cast<BvType>(&expr->getType())->getWidth()
        ));
    }

    Z3AstHandle visitFpToUnsigned(const ExprRef<FpToUnsignedExpr>& expr) override
    {
        return createHandle(Z3_mk_fpa_to_ubv(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            llvm::cast<BvType>(&expr->getType())->getWidth()
        ));
    }

    Z3AstHandle visitFpToBv(const ExprRef<FpToBvExpr>& expr) override
    {
        return createHandle(Z3_mk_fpa_to_ieee_bv(
            mZ3Context,
            getOperand(0)
        ));
    }

    Z3AstHandle visitBvToFp(const ExprRef<BvToFpExpr>& expr) override
    {
        return createHandle(Z3_mk_fpa_to_fp_bv(
            mZ3Context,
            getOperand(0),
            typeToSort(expr->getType())
        ));
    }

    Z3AstHandle visitTupleSelect(const ExprRef<TupleSelectExpr>& expr) override;
    Z3AstHandle visitTupleConstruct(const ExprRef<TupleConstructExpr>& expr) override;

    Z3AstHandle transformRoundingMode(llvm::APFloat::roundingMode rm);
    Z3AstHandle translateAPInt(const llvm::APInt& value, Z3Handle<Z3_sort> z3Sort);

protected:
    Z3_context& mZ3Context;
    unsigned& mTmpCount;
    Z3CacheMapTy& mCache;
    Z3DeclMapTy& mDecls;
    std::unordered_map<const TupleType*, TupleInfo> mTupleInfo;
};

/// Z3 solver implementation
class Z3Solver : public Solver
{
public:
    explicit Z3Solver(GazerContext& context);

    Z3Solver(const Z3Solver&) = delete;
    Z3Solver& operator=(const Z3Solver&) = delete;

    void printStats(llvm::raw_ostream& os) override;
    void dump(llvm::raw_ostream& os) override;
    SolverStatus run() override;
    
    std::unique_ptr<Model> getModel() override;

    ~Z3Solver() override;

protected:
    void addConstraint(ExprPtr expr) override;

    void doReset() override;

    void doPush() override;
    void doPop() override;

private:
    Z3_config mConfig;
    Z3_context mZ3Context;
    Z3_solver mSolver;
    unsigned mTmpCount = 0;
    Z3CacheMapTy mCache;
    Z3DeclMapTy mDecls;
    Z3ExprTransformer mTransformer;
};

} // end namespace gazer

#endif