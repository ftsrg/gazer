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
#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Core/Expr/ExprWalker.h"
#include "gazer/ADT/ScopedCache.h"
#include "gazer/Support/Float.h"

#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Debug.h>

#include <z3++.h>

#define DEBUG_TYPE "Z3Solver"

using namespace gazer;

namespace
{
    llvm::cl::opt<bool> Z3SolveParallel("z3-solve-parallel", llvm::cl::desc("Enable Z3's parallel solver"));
    llvm::cl::opt<int>  Z3ThreadsMax("z3-threads-max", llvm::cl::desc("Maximum number of threads"), llvm::cl::init(0));
} // end anonymous namespace

namespace
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

    /*implicit*/ operator T()
    {
        assert(mContext != nullptr);
        assert(mNode != nullptr);

        return mNode;
    }

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

using Z3AstHandle = Z3Handle<Z3_ast>;
using CacheMapT = ScopedCache<ExprPtr, Z3AstHandle, std::unordered_map<ExprPtr, Z3AstHandle>>;

class Z3ExprTransformer : public ExprWalker<Z3ExprTransformer, Z3AstHandle>
{
    friend class ExprWalker<Z3ExprTransformer, Z3AstHandle>;

    struct TupleInfo
    {
        Z3Handle<Z3_sort> sort;
        Z3Handle<Z3_func_decl> constructor;
        std::vector<Z3Handle<Z3_func_decl>> projections;
    };
public:
    Z3ExprTransformer(z3::context& context, unsigned& tmpCount, CacheMapT& cache)
        : mZ3Context(context), mTmpCount(tmpCount), mCache(cache)
    {}

protected:
    Z3AstHandle createHandle(Z3_ast ast)
    {
        #ifndef NDEBUG
        if (ast == nullptr) {
            std::string errStr;
            llvm::raw_string_ostream err{errStr};

            err << "Invalid Z3_ast!\n"
                << "Z3 error: " << Z3_get_error_msg(mZ3Context, Z3_get_error_code(mZ3Context))
                << "\n";
            llvm::report_fatal_error(err.str(), true);
        }
        #endif

        return Z3AstHandle{mZ3Context, ast};
    }

    Z3Handle<Z3_sort> typeToSort(Type& type);
    Z3Handle<Z3_sort> handleTupleType(TupleType& tupType);

    Z3AstHandle translateLiteral(const ExprRef<LiteralExpr>& expr);

private:
    bool shouldSkip(const ExprPtr& expr, Z3AstHandle* ret)
    {
        if (expr->isNullary() || expr->isUnary()) {
            return false;
        }

        auto result = mCache.get(expr);
        if (result) {
            *ret = *result;
            return true;
        }

        return false;
    }
    
    void handleResult(const ExprPtr& expr, Z3AstHandle& ret)
    {
        mCache.insert(expr, ret);
    }

    Z3AstHandle visitExpr(const ExprPtr& expr) // NOLINT(readability-convert-member-functions-to-static)
    {
        llvm_unreachable("Unhandled expression type in Z3ExprTransformer.");
    }

    Z3AstHandle visitUndef(const ExprRef<UndefExpr>& expr)
    {
        return createHandle(
            Z3_mk_fresh_const(mZ3Context, "", typeToSort(expr->getType()))
        );
    }

    Z3AstHandle visitLiteral(const ExprRef<LiteralExpr>& expr) {
        return this->translateLiteral(expr);
    }

    Z3AstHandle visitVarRef(const ExprRef<VarRefExpr>& expr)
    {
        auto name = expr->getVariable().getName();
        return createHandle(
            Z3_mk_const(mZ3Context, Z3_mk_string_symbol(mZ3Context, name.c_str()), typeToSort(expr->getType()))
        );
    }

    // Use some helper macros to translate trivial cases
    #define TRANSLATE_UNARY_OP(NAME, Z3_METHOD)                                     \
    Z3AstHandle visit##NAME(const ExprRef<NAME##Expr>& expr) {                      \
        return createHandle(Z3_METHOD(mZ3Context, getOperand(0)));                  \
    }                                                                               \

    #define TRANSLATE_BINARY_OP(NAME, Z3_METHOD)                                    \
    Z3AstHandle visit##NAME(const ExprRef<NAME##Expr>& expr) {                      \
        return createHandle(Z3_METHOD(mZ3Context, getOperand(0), getOperand(1)));   \
    }                                                                               \

    // Unary logic
    TRANSLATE_UNARY_OP(Not,         Z3_mk_not)

    // Arithmetic operators
    TRANSLATE_BINARY_OP(Div,        Z3_mk_div)
    TRANSLATE_BINARY_OP(Mod,        Z3_mk_mod)
    TRANSLATE_BINARY_OP(Rem,        Z3_mk_rem)

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

    // Arithmetic comparisons
    TRANSLATE_BINARY_OP(Lt,         Z3_mk_lt)
    TRANSLATE_BINARY_OP(LtEq,       Z3_mk_le)
    TRANSLATE_BINARY_OP(Gt,         Z3_mk_gt)
    TRANSLATE_BINARY_OP(GtEq,       Z3_mk_ge)

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

    #undef TRANSLATE_UNARY_OP
    #undef TRANSLATE_BINARY_OP

    // Bit-vector casts
    Z3AstHandle visitZExt(const ExprRef<ZExtExpr>& expr)
    {
        return createHandle(Z3_mk_zero_ext(mZ3Context, expr->getWidthDiff(), getOperand(0)));
    }

    Z3AstHandle visitSExt(const ExprRef<SExtExpr>& expr)
    {
        return createHandle(Z3_mk_sign_ext(mZ3Context, expr->getWidthDiff(), getOperand(0)));
    }

    Z3AstHandle visitExtract(const ExprRef<ExtractExpr>& expr)
    {
        unsigned hi = expr->getOffset() + expr->getWidth() - 1;
        unsigned lo = expr->getOffset();

        return createHandle(Z3_mk_extract(mZ3Context, hi, lo, getOperand(0)));
    }

    // Overloaded binary arithmetic
    Z3AstHandle visitAdd(const ExprRef<AddExpr>& expr)
    {
        if (expr->getType().isBvType()) {
            return createHandle(Z3_mk_bvadd(mZ3Context, getOperand(0), getOperand(1)));
        }

        std::array<Z3_ast, 2> ops = { getOperand(0), getOperand(1) };
        return createHandle(Z3_mk_add(mZ3Context, 2, ops.data()));
    }

    Z3AstHandle visitSub(const ExprRef<SubExpr>& expr)
    {
        if (expr->getType().isBvType()) {
            return createHandle(Z3_mk_bvsub(mZ3Context, getOperand(0), getOperand(1)));
        }

        std::array<Z3_ast, 2> ops = { getOperand(0), getOperand(1) };
        return createHandle(Z3_mk_sub(mZ3Context, 2, ops.data()));
    }

    Z3AstHandle visitMul(const ExprRef<MulExpr>& expr)
    {
        if (expr->getType().isBvType()) {
            return createHandle(Z3_mk_bvmul(mZ3Context, getOperand(0), getOperand(1)));
        }

        std::array<Z3_ast, 2> ops = { getOperand(0), getOperand(1) };
        return createHandle(Z3_mk_mul(mZ3Context, 2, ops.data()));
    }

    // Multiary logic
    Z3AstHandle visitAnd(const ExprRef<AndExpr>& expr)
    {
        z3::array<Z3_ast> ops(expr->getNumOperands());
        for (size_t i = 0; i < ops.size(); ++i) {
            ops[i] = getOperand(i);
        }
    
        return createHandle(Z3_mk_and(mZ3Context, expr->getNumOperands(), ops.ptr()));
    }

    Z3AstHandle visitOr(const ExprRef<OrExpr>& expr)
    {
        z3::array<Z3_ast> ops(expr->getNumOperands());
        for (size_t i = 0; i < expr->getNumOperands(); ++i) {
            ops[i] = getOperand(i);
        }
    
        return createHandle(Z3_mk_or(mZ3Context, expr->getNumOperands(), ops.ptr()));
    }

    Z3AstHandle visitXor(const ExprRef<XorExpr>& expr)
    {
        assert(expr->getType().isBoolType() && "Can only handle boolean XORs");

        std::array<Z3_ast, 2> ops = { getOperand(0), getOperand(1) };
        return createHandle(Z3_mk_distinct(mZ3Context, 2, ops.data()));
    }

    Z3AstHandle visitImply(const ExprRef<ImplyExpr>& expr)
    {
        assert(expr->getType().isBoolType() && "Can only handle boolean implications");

        return createHandle(Z3_mk_implies(mZ3Context, getOperand(0), getOperand(1)));
    }

    // Compare
    Z3AstHandle visitEq(const ExprRef<EqExpr>& expr)
    {
        return createHandle(Z3_mk_eq(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitNotEq(const ExprRef<NotEqExpr>& expr)
    {
        std::array<Z3_ast, 2> ops = { getOperand(0), getOperand(1) };
        return createHandle(Z3_mk_distinct(mZ3Context, 2, ops.data()));
    }

    // Floating-point casts
    Z3AstHandle visitFCast(const ExprRef<FCastExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_to_fp_float(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            typeToSort(expr->getType())  
        ));
    }

    Z3AstHandle visitSignedToFp(const ExprRef<SignedToFpExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_to_fp_signed(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            typeToSort(expr->getType())
        ));
    }

    Z3AstHandle visitUnsignedToFp(const ExprRef<UnsignedToFpExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_to_fp_unsigned(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            typeToSort(expr->getType())
        ));
    }

    Z3AstHandle visitFpToSigned(const ExprRef<FpToSignedExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_to_sbv(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            llvm::cast<BvType>(&expr->getType())->getWidth()
        ));
    }

    Z3AstHandle visitFpToUnsigned(const ExprRef<FpToUnsignedExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_to_ubv(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            llvm::cast<BvType>(&expr->getType())->getWidth()
        ));
    }

    // Floating-point arithmetic

    Z3AstHandle visitFAdd(const ExprRef<FAddExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_add(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0), getOperand(1)
        ));
    }

    Z3AstHandle visitFSub(const ExprRef<FSubExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_sub(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0), getOperand(1)
        ));
    }

    Z3AstHandle visitFMul(const ExprRef<FMulExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_mul(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0), getOperand(1)
        ));
    }

    Z3AstHandle visitFDiv(const ExprRef<FDivExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_div(mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0), getOperand(1)
        ));
    }

    // Ternary
    Z3AstHandle visitSelect(const ExprRef<SelectExpr>& expr)
    {
        return createHandle(Z3_mk_ite(mZ3Context,
            getOperand(0), // condition
            getOperand(1), // then
            getOperand(2)  // else
        ));
    }

    // Arrays
    Z3AstHandle visitArrayRead(const ExprRef<ArrayReadExpr>& expr)
    {
        return createHandle(Z3_mk_select(mZ3Context,
            getOperand(0), getOperand(1)
        ));
    }

    Z3AstHandle visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr)
    {
        return createHandle(Z3_mk_store(mZ3Context,
            getOperand(0), getOperand(1), getOperand(2)
        ));
    }

    Z3AstHandle visitTupleSelect(const ExprRef<TupleSelectExpr>& expr)
    {
        auto& tupTy = llvm::cast<TupleType>(expr->getOperand(0)->getType());
        
        assert(mTupleInfo.count(&tupTy) != 0 && "Tuple types should have been handled before!");
        auto& info = mTupleInfo[&tupTy];

        auto& proj = info.projections[expr->getIndex()];
        std::array<Z3_ast, 1> args = { getOperand(0) };

        return createHandle(Z3_mk_app(mZ3Context, proj, 1, args.data()));
    }

    Z3AstHandle visitTupleConstruct(const ExprRef<TupleConstructExpr>& expr)
    {
        auto& tupTy = llvm::cast<TupleType>(expr->getOperand(0)->getType());
        
        assert(mTupleInfo.count(&tupTy) != 0 && "Tuple types should have been handled before!");
        auto& info = mTupleInfo[&tupTy];

        std::vector<Z3_ast> args(tupTy.getNumSubtypes());
        for (size_t i = 0; i < tupTy.getNumSubtypes(); ++i) {
            args[i] = getOperand(i);
        }

        return createHandle(Z3_mk_app(mZ3Context, info.constructor, tupTy.getNumSubtypes(), args.data()));
    }

protected:
    Z3AstHandle transformRoundingMode(llvm::APFloat::roundingMode rm)
    {
        switch (rm) {
            case llvm::APFloat::roundingMode::rmNearestTiesToEven:
                return createHandle(Z3_mk_fpa_round_nearest_ties_to_even(mZ3Context));
            case llvm::APFloat::roundingMode::rmNearestTiesToAway:
                return createHandle(Z3_mk_fpa_round_nearest_ties_to_away(mZ3Context));
            case llvm::APFloat::roundingMode::rmTowardPositive:
                return createHandle(Z3_mk_fpa_round_toward_positive(mZ3Context));
            case llvm::APFloat::roundingMode::rmTowardNegative:
                return createHandle(Z3_mk_fpa_round_toward_negative(mZ3Context));
            case llvm::APFloat::roundingMode::rmTowardZero:
                return createHandle(Z3_mk_fpa_round_toward_zero(mZ3Context));
        }

        llvm_unreachable("Invalid rounding mode");
    }

protected:
    z3::context& mZ3Context;
    unsigned& mTmpCount;
    CacheMapT& mCache;
    std::unordered_map<const TupleType*, TupleInfo> mTupleInfo;
};

/// Z3 solver implementation.
class Z3Solver : public Solver
{
public:
    explicit Z3Solver(GazerContext& context)
        : Solver(context), mSolver(mZ3Context),
        mTransformer(mZ3Context, mTmpCount, mCache)
    {
        if (Z3SolveParallel) {
            mZ3Context.set("parallel.enable", true);
            if (Z3ThreadsMax != 0) {
                mZ3Context.set("threads.max", Z3ThreadsMax);
            }
        }
    }

    void printStats(llvm::raw_ostream& os) override;
    void dump(llvm::raw_ostream& os) override;
    SolverStatus run() override;
    Valuation getModel() override;
    void reset() override;

    void push() override;
    void pop() override;

protected:
    void addConstraint(ExprPtr expr) override;

protected:
    z3::context mZ3Context;
    z3::solver mSolver;
    unsigned mTmpCount = 0;
    CacheMapT mCache;
    Z3ExprTransformer mTransformer;
};

} // end anonymous namespace

// Z3ExprTransformer implementation
//===----------------------------------------------------------------------===//
Z3Handle<Z3_sort> Z3ExprTransformer::typeToSort(Type& type)
{
    switch (type.getTypeID()) {
        case Type::BoolTypeID:
            return Z3Handle<Z3_sort>{mZ3Context, Z3_mk_bool_sort(mZ3Context)};
        case Type::IntTypeID:
            return Z3Handle<Z3_sort>{mZ3Context, Z3_mk_int_sort(mZ3Context)};
        case Type::BvTypeID: {
            auto& intTy = llvm::cast<BvType>(type);
            return Z3Handle<Z3_sort>{mZ3Context, Z3_mk_bv_sort(mZ3Context, intTy.getWidth())};
        }
        case Type::RealTypeID:
            return Z3Handle<Z3_sort>{mZ3Context, Z3_mk_real_sort(mZ3Context)};
        case Type::FloatTypeID: {
            auto& fltTy = llvm::cast<FloatType>(type);
            switch (fltTy.getPrecision()) {
                case FloatType::Half:
                    return Z3Handle<Z3_sort>(mZ3Context, Z3_mk_fpa_sort_half(mZ3Context));
                case FloatType::Single:
                    return Z3Handle<Z3_sort>(mZ3Context, Z3_mk_fpa_sort_single(mZ3Context));
                case FloatType::Double:
                    return Z3Handle<Z3_sort>(mZ3Context, Z3_mk_fpa_sort_double(mZ3Context));
                case FloatType::Quad:
                    return Z3Handle<Z3_sort>(mZ3Context, Z3_mk_fpa_sort_quadruple(mZ3Context));
            }
            llvm_unreachable("Invalid floating-point precision");
        }
        case Type::ArrayTypeID: {
            auto& arrTy = llvm::cast<ArrayType>(type);
            return Z3Handle<Z3_sort>(mZ3Context, Z3_mk_array_sort(mZ3Context,
                typeToSort(arrTy.getIndexType()),
                typeToSort(arrTy.getElementType())
            ));
        }
        case Type::TupleTypeID:
            return this->handleTupleType(llvm::cast<TupleType>(type));
    }

    llvm_unreachable("Unsupported gazer type for Z3Solver");
}

Z3Handle<Z3_sort> Z3ExprTransformer::handleTupleType(TupleType& tupTy)
{
    // Check if we already visited this type
    auto it = mTupleInfo.find(&tupTy);
    if (it != mTupleInfo.end()) {
        return it->second.sort;
    }

    auto name = Z3_mk_string_symbol(mZ3Context, ("mk_" + tupTy.getName()).c_str());

    std::vector<Z3_symbol> projs;
    std::vector<Z3_sort> sorts;

    for (size_t i = 0; i < tupTy.getNumSubtypes(); ++i) {
        std::string projName = tupTy.getName() + "_" + std::to_string(i);
        projs.emplace_back(Z3_mk_string_symbol(mZ3Context, projName.c_str()));
        sorts.emplace_back(typeToSort(tupTy.getTypeAtIndex(i)));
    }

    Z3_func_decl constructDecl;
    std::vector<Z3_func_decl> projDecls(tupTy.getNumSubtypes());

    auto tup = Z3_mk_tuple_sort(
        mZ3Context, name, tupTy.getNumSubtypes(), projs.data(),
        sorts.data(), &constructDecl, projDecls.data()
    );

    if (tup == nullptr) {
        std::string errStr;
        llvm::raw_string_ostream err{errStr};

        err << "Invalid Z3_sort!\n"
            << "Z3 error: " << Z3_get_error_msg(mZ3Context, Z3_get_error_code(mZ3Context))
            << "\n";
        llvm::report_fatal_error(err.str(), true);
    }

    auto& info = mTupleInfo[&tupTy];

    info.constructor = Z3Handle<Z3_func_decl>(mZ3Context, constructDecl);
    for (size_t i = 0; i < tupTy.getNumSubtypes(); ++i) {
        info.projections.emplace_back(Z3Handle<Z3_func_decl>(mZ3Context, projDecls[i]));
    }
    info.sort = Z3Handle<Z3_sort>(mZ3Context, tup);

    return info.sort;
}

Z3AstHandle Z3ExprTransformer::translateLiteral(const ExprRef<LiteralExpr>& expr)
{
    if (auto bvLit = llvm::dyn_cast<BvLiteralExpr>(expr)) {
        return createHandle(Z3_mk_unsigned_int64(
            mZ3Context, bvLit->getValue().getZExtValue(), typeToSort(bvLit->getType())
        ));
    }
    
    if (auto bl = llvm::dyn_cast<BoolLiteralExpr>(expr)) {
        return createHandle(bl->getValue() ? Z3_mk_true(mZ3Context) : Z3_mk_false(mZ3Context));
    }
    
    if (auto il = llvm::dyn_cast<IntLiteralExpr>(expr)) {
        return createHandle(Z3_mk_int64(mZ3Context, il->getValue(), Z3_mk_int_sort(mZ3Context)));
    }
    
    if (auto fl = llvm::dyn_cast<FloatLiteralExpr>(expr)) {        
        if (fl->getType().getPrecision() == FloatType::Single) {
            return createHandle(Z3_mk_fpa_numeral_float(
                mZ3Context, fl->getValue().convertToFloat(), typeToSort(fl->getType())
            ));
        }
        
        if (fl->getType().getPrecision() == FloatType::Double) {
            return createHandle(Z3_mk_fpa_numeral_double(
                mZ3Context, fl->getValue().convertToDouble(), typeToSort(fl->getType())
            ));
        }
    }

    if (auto arrayLit = llvm::dyn_cast<ArrayLiteralExpr>(expr)) {
        Z3AstHandle ast;            
        if (arrayLit->hasDefault()) {
            ast = createHandle(Z3_mk_const_array(
                mZ3Context,
                typeToSort(arrayLit->getType().getIndexType()),
                this->translateLiteral(arrayLit->getDefault())
            ));
        } else {
            ast = createHandle(Z3_mk_fresh_const(
                mZ3Context,
                "",
                typeToSort(arrayLit->getType())
            ));
        }

        Z3AstHandle result = ast;
        for (auto& [index, elem] : arrayLit->getMap()) {
            result = createHandle(Z3_mk_store(
                mZ3Context, result, this->translateLiteral(index), this->translateLiteral(elem)
            ));
        }

        return result;
    }

    llvm_unreachable("Unsupported operand type.");
}

Solver::SolverStatus Z3Solver::run()
{
    z3::check_result result = mSolver.check();

    switch (result) {
        case z3::unsat: return SolverStatus::UNSAT;
        case z3::sat: return SolverStatus::SAT;
        case z3::unknown: return SolverStatus::UNKNOWN;
    }

    llvm_unreachable("Unknown solver status encountered.");
}

void Z3Solver::addConstraint(ExprPtr expr)
{
    auto z3Expr = mTransformer.walk(expr);
    mSolver.add(z3::expr(mZ3Context, z3Expr));
}

void Z3Solver::reset()
{
    mCache.clear();
    mSolver.reset();
}

void Z3Solver::push()
{
    mCache.push();
    mSolver.push();
}

void Z3Solver::pop()
{
    mCache.pop();
    mSolver.pop();
}

void Z3Solver::printStats(llvm::raw_ostream& os)
{    
    std::stringstream ss;
    ss << mSolver.statistics();
    os << ss.str();
}

void Z3Solver::dump(llvm::raw_ostream& os)
{
    os << Z3_solver_to_string(mZ3Context, mSolver);
}

//---- Support for model extraction ----//

static FloatType::FloatPrecision precFromSort(z3::context& context, const z3::sort& sort)
{
    assert(sort.sort_kind() == Z3_sort_kind::Z3_FLOATING_POINT_SORT);

    unsigned width = Z3_fpa_get_ebits(context, sort) + Z3_fpa_get_sbits(context, sort);

    switch (width) {
        case FloatType::Half:   return FloatType::Half;
        case FloatType::Single: return FloatType::Single;
        case FloatType::Double: return FloatType::Double;
        case FloatType::Quad:   return FloatType::Quad;
    }

    llvm_unreachable("Invalid floating point type!");
}

llvm::APInt z3_bv_to_apint(z3::context& context, z3::model& model, const z3::expr& bv)
{
    assert(bv.is_bv() && "Bitvector conversion requires a bitvector");
    unsigned int width = Z3_get_bv_sort_size(context, bv.get_sort());

    if (width <= 64) { //NOLINT(readability-magic-numbers)
        uint64_t value;
        Z3_get_numeral_uint64(context, bv, &value);

        return llvm::APInt(width, value);
    }
    
    llvm::SmallVector<uint64_t, 2> bits;
    for (size_t i = 0; i < width; i += 64) { //NOLINT(readability-magic-numbers)
        uint64_t value;
        Z3_get_numeral_uint64(context, model.eval(bv.extract(i, 64)), &value);

        bits.push_back(value);
    }

    return llvm::APInt(width, bits);
}

Valuation Z3Solver::getModel()
{
    // TODO: Check whether the formula is SAT
    auto builder = Valuation::CreateBuilder();
    z3::model model = mSolver.get_model();

    LLVM_DEBUG(llvm::dbgs() << Z3_model_to_string(mZ3Context, model) << "\n");

    for (size_t i = 0; i < model.num_consts(); ++i) {
        z3::func_decl decl = model.get_const_decl(i);
        z3::expr z3Expr = model.get_const_interp(decl);

        auto name = decl.name().str();
        auto variableOpt = mContext.getVariable(name);
        if (variableOpt == nullptr) {
            LLVM_DEBUG(llvm::dbgs() << "Model: skipping variable '" << name << "'\n");
            // The given Gazer context does not contain this variable, must be
            // an auxiliary introduced here.
            continue;
        }

        Variable& variable = *variableOpt;
        ExprRef<LiteralExpr> expr = nullptr;

        if (z3Expr.is_bool()) {
            bool value = z3::eq(model.eval(z3Expr), mZ3Context.bool_val(true));
            expr = BoolLiteralExpr::Get(getContext(), value);
        } else if (z3Expr.is_int()) {
            // TODO: Maybe try with Z3_get_numeral_string?
            int64_t value;
            Z3_get_numeral_int64(mZ3Context, model.eval(z3Expr), &value);

            const Type* varTy = &variable.getType();
            assert(varTy->isIntType() && "An IntType should only be contained in an IntType variable.");
        
            expr = IntLiteralExpr::Get(IntType::Get(getContext()), value);
        } else if (z3Expr.is_bv()) {
            unsigned int width = Z3_get_bv_sort_size(mZ3Context, z3Expr.get_sort());
            uint64_t value;
            Z3_get_numeral_uint64(mZ3Context, z3Expr, &value);

            llvm::APInt iVal(width, value);
            expr = BvLiteralExpr::Get(BvType::Get(getContext(), width), iVal);
        } else if (z3Expr.get_sort().sort_kind() == Z3_sort_kind::Z3_FLOATING_POINT_SORT) {
            z3::sort sort = z3Expr.get_sort();
            FloatType::FloatPrecision precision = precFromSort(mZ3Context, sort);
            auto& fltTy = FloatType::Get(getContext(), precision);

            bool isNaN = z3::eq(
                model.eval(z3::expr(mZ3Context, Z3_mk_fpa_is_nan(mZ3Context, z3Expr))),
                mZ3Context.bool_val(true)
            );

            if (isNaN) {
                expr = FloatLiteralExpr::Get(fltTy, llvm::APFloat::getNaN(
                    fltTy.getLLVMSemantics()
                ));
            } else {
                auto toIEEE = z3::expr(mZ3Context, Z3_mk_fpa_to_ieee_bv(mZ3Context, z3Expr));
                auto ieeeVal = model.eval(toIEEE);

                uint64_t bits;
                Z3_get_numeral_uint64(mZ3Context,  ieeeVal, &bits);

                llvm::APInt bv(fltTy.getWidth(), bits);
                llvm::APFloat apflt(fltTy.getLLVMSemantics(), bv);

                expr = FloatLiteralExpr::Get(fltTy, apflt);
            }
        } else if (z3Expr.get_sort().sort_kind() == Z3_sort_kind::Z3_ARRAY_SORT) {
            // TODO
            continue;
        } else {
            llvm_unreachable("Unhandled Z3 expression type.");
        }

        builder.put(&variable, expr);
    }

    return builder.build();
}

std::unique_ptr<Solver> Z3SolverFactory::createSolver(GazerContext& context)
{
    return std::unique_ptr<Solver>(new Z3Solver(context));
}
