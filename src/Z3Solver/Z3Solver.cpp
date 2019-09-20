#include "gazer/Z3Solver/Z3Solver.h"

#include "gazer/Core/Expr/ExprWalker.h"

#include "gazer/ADT/ScopedCache.h"
#include "gazer/Support/Float.h"

#include <llvm/Support/raw_os_ostream.h>

#include <z3++.h>

using namespace gazer;

namespace
{

// Implementation based upon Z3NodeHandle in the KLEE project:
// https://github.com/klee/klee/blob/master/lib/Solver/Z3Builder.h
template<class T>
class Z3Handle
{
public:
    Z3Handle()
        : mContext(nullptr), mNode(nullptr)
    {}

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
    
    Z3Handle(Z3Handle&& other)
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

    Z3Handle& operator=(Z3Handle&& other)
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
    Z3_context mContext;
    T mNode;
};


template<> inline Z3_ast Z3Handle<Z3_sort>::as_ast() {
    return Z3_sort_to_ast(mContext, mNode);
}

template<> inline Z3_ast Z3Handle<Z3_ast>::as_ast() {
    return mNode;
}

using Z3AstHandle = Z3Handle<Z3_ast>;

class Z3Solver : public Solver
{
public:
    using CacheMapT = ScopedCache<ExprPtr, Z3AstHandle, std::unordered_map<ExprPtr, Z3AstHandle>>;
    
public:
    Z3Solver(GazerContext& context)
        : Solver(context), mSolver(mZ3Context)
    {}

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
};

class Z3ExprTransformer : public ExprWalker<Z3ExprTransformer, Z3AstHandle>
{
public:
    Z3ExprTransformer(z3::context& context, unsigned& tmpCount, Z3Solver::CacheMapT& cache)
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

    Z3Handle<Z3_sort> typeToSort(const Type* type)
    {
        switch (type->getTypeID()) {
            case Type::BoolTypeID:
                return Z3Handle<Z3_sort>{mZ3Context, Z3_mk_bool_sort(mZ3Context)};
            case Type::IntTypeID:
                return Z3Handle<Z3_sort>{mZ3Context, Z3_mk_int_sort(mZ3Context)};
            case Type::BvTypeID: {
                auto intTy = llvm::cast<BvType>(type);
                return Z3Handle<Z3_sort>{mZ3Context, Z3_mk_bv_sort(mZ3Context, intTy->getWidth())};
            }
            case Type::RealTypeID:
                return Z3Handle<Z3_sort>{mZ3Context, Z3_mk_real_sort(mZ3Context)};
            case Type::FloatTypeID: {
                auto fltTy = llvm::cast<FloatType>(type);
                switch (fltTy->getPrecision()) {
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
                auto arrTy = llvm::cast<ArrayType>(type);
                return Z3Handle<Z3_sort>(mZ3Context, Z3_mk_array_sort(mZ3Context,
                    typeToSort(&arrTy->getIndexType()),
                    typeToSort(&arrTy->getElementType())
                ));
            }
        }

        llvm_unreachable("Unsupported gazer type for Z3Solver");
    }
public:
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

    Z3AstHandle visitExpr(const ExprPtr& expr)
    {
        llvm_unreachable("Unhandled expression type in Z3ExprTransformer.");
    }

    Z3AstHandle visitUndef(const ExprRef<UndefExpr>& expr)
    {
        std::string name = "__gazer_undef:" + std::to_string(mTmpCount++);

        return createHandle(
            Z3_mk_const(mZ3Context, Z3_mk_string_symbol(mZ3Context, name.c_str()), typeToSort(&expr->getType()))
        );
    }

    Z3AstHandle visitLiteral(const ExprRef<LiteralExpr>& expr)
    {
        if (expr->getType().isBvType()) {
            auto lit = llvm::dyn_cast<BvLiteralExpr>(&*expr);
            auto value = lit->getValue();

            return createHandle(
                Z3_mk_unsigned_int64(mZ3Context, value.getLimitedValue(), typeToSort(&lit->getType()))
            );
        }
        
        if (expr->getType().isBoolType()) {
            auto value = llvm::dyn_cast<BoolLiteralExpr>(&*expr)->getValue();
            return createHandle(value ? Z3_mk_true(mZ3Context) : Z3_mk_false(mZ3Context));
        }
        
        if (expr->getType().isIntType()) {
            int64_t value = llvm::dyn_cast<IntLiteralExpr>(&*expr)->getValue();
            return createHandle(
                Z3_mk_int64(mZ3Context, value, Z3_mk_int_sort(mZ3Context))
            );
        }
        
        if (expr->getType().isFloatType()) {
            auto fltTy = llvm::dyn_cast<FloatType>(&expr->getType());
            auto value = llvm::dyn_cast<FloatLiteralExpr>(&*expr)->getValue();
            
            if (fltTy->getPrecision() == FloatType::Single) {
                return createHandle(Z3_mk_fpa_numeral_float(
                    mZ3Context, value.convertToFloat(), typeToSort(fltTy)
                ));
            } else if (fltTy->getPrecision() == FloatType::Double) {
                return createHandle(Z3_mk_fpa_numeral_double(
                    mZ3Context, value.convertToDouble(), typeToSort(fltTy)
                ));
            }
        }

        llvm_unreachable("Unsupported operand type.");
    }

    Z3AstHandle visitVarRef(const ExprRef<VarRefExpr>& expr)
    {
        auto name = expr->getVariable().getName();
        return createHandle(
            Z3_mk_const(mZ3Context, Z3_mk_string_symbol(mZ3Context, name.c_str()), typeToSort(&expr->getType()))
        );
    }

    // Unary
    Z3AstHandle visitNot(const ExprRef<NotExpr>& expr)
    {
        return createHandle(Z3_mk_not(mZ3Context, getOperand(0)));
    }

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

    // Binary
    Z3AstHandle visitAdd(const ExprRef<AddExpr>& expr)
    {
        if (expr->getType().isBvType()) {
            return createHandle(Z3_mk_bvadd(mZ3Context, getOperand(0), getOperand(1)));
        }

        Z3_ast ops[2];
        ops[0] = getOperand(0);
        ops[1] = getOperand(1);

        return createHandle(Z3_mk_add(mZ3Context, 2, ops));
    }

    Z3AstHandle visitSub(const ExprRef<SubExpr>& expr)
    {
        if (expr->getType().isBvType()) {
            return createHandle(Z3_mk_bvsub(mZ3Context, getOperand(0), getOperand(1)));
        }

        Z3_ast ops[2];
        ops[0] = getOperand(0);
        ops[1] = getOperand(1);

        return createHandle(Z3_mk_sub(mZ3Context, 2, ops));
    }

    Z3AstHandle visitMul(const ExprRef<MulExpr>& expr)
    {
        if (expr->getType().isBvType()) {
            return createHandle(Z3_mk_bvmul(mZ3Context, getOperand(0), getOperand(1)));
        }

        Z3_ast ops[2];
        ops[0] = getOperand(0);
        ops[1] = getOperand(1);

        return createHandle(Z3_mk_mul(mZ3Context, 2, ops));
    }

    Z3AstHandle visitBvSDiv(const ExprRef<BvSDivExpr>& expr)
    {
        return createHandle(Z3_mk_bvsdiv(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvUDiv(const ExprRef<BvUDivExpr>& expr)
    {
        return createHandle(Z3_mk_bvudiv(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvSRem(const ExprRef<BvSRemExpr>& expr)
    {
        return createHandle(Z3_mk_bvsrem(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvURem(const ExprRef<BvURemExpr>& expr)
    {
        return createHandle(Z3_mk_bvurem(mZ3Context, getOperand(0), getOperand(1)));
    }    

    Z3AstHandle visitShl(const ExprRef<ShlExpr>& expr)
    {
        return createHandle(Z3_mk_bvshl(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitLShr(const ExprRef<LShrExpr>& expr)
    {
        return createHandle(Z3_mk_bvlshr(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitAShr(const ExprRef<AShrExpr>& expr)
    {
        return createHandle(Z3_mk_bvashr(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvAnd(const ExprRef<BvAndExpr>& expr)
    {
        return createHandle(Z3_mk_bvand(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvOr(const ExprRef<BvOrExpr>& expr)
    {
        return createHandle(Z3_mk_bvor(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvXor(const ExprRef<BvXorExpr>& expr)
    {
        return createHandle(Z3_mk_bvxor(mZ3Context, getOperand(0), getOperand(1)));
    }

    // Logic
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
        Z3_ast ops[2] = { getOperand(0), getOperand(1) };

        return createHandle(Z3_mk_distinct(mZ3Context, 2, ops));
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
        Z3_ast ops[2] = { getOperand(0), getOperand(1) };
        return createHandle(Z3_mk_distinct(mZ3Context, 2, ops));
    }

    Z3AstHandle visitLt(const ExprRef<LtExpr>& expr)
    {
        return createHandle(Z3_mk_lt(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitLtEq(const ExprRef<LtEqExpr>& expr)
    {
        return createHandle(Z3_mk_le(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitGt(const ExprRef<GtExpr>& expr)
    {
        return createHandle(Z3_mk_gt(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitGtEq(const ExprRef<GtEqExpr>& expr)
    {
        return createHandle(Z3_mk_ge(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvSLt(const ExprRef<BvSLtExpr>& expr)
    {
        return createHandle(Z3_mk_bvslt(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvSLtEq(const ExprRef<BvSLtEqExpr>& expr)
    {
        return createHandle(Z3_mk_bvsle(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvSGt(const ExprRef<BvSGtExpr>& expr)
    {
        return createHandle(Z3_mk_bvsgt(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvSGtEq(const ExprRef<BvSGtEqExpr>& expr)
    {
        return createHandle(Z3_mk_bvsge(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvULt(const ExprRef<BvULtExpr>& expr)
    {
        return createHandle(Z3_mk_bvult(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvULtEq(const ExprRef<BvULtEqExpr>& expr)
    {
        return createHandle(Z3_mk_bvule(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvUGt(const ExprRef<BvUGtExpr>& expr)
    {
        return createHandle(Z3_mk_bvugt(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitBvUGtEq(const ExprRef<BvUGtEqExpr>& expr)
    {
        return createHandle(Z3_mk_bvuge(mZ3Context, getOperand(0), getOperand(1)));
    }

    // Floating-point queries
    Z3AstHandle visitFIsNan(const ExprRef<FIsNanExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_is_nan(mZ3Context, getOperand(0)));
    }

    Z3AstHandle visitFIsInf(const ExprRef<FIsInfExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_is_infinite(mZ3Context, getOperand(0)));
    }

    // Floating-point casts
    Z3AstHandle visitFCast(const ExprRef<FCastExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_to_fp_float(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            typeToSort(&expr->getType())  
        ));
    }

    Z3AstHandle visitSignedToFp(const ExprRef<SignedToFpExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_to_fp_signed(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            typeToSort(&expr->getType())
        ));
    }

    Z3AstHandle visitUnsignedToFp(const ExprRef<UnsignedToFpExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_to_fp_unsigned(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            typeToSort(&expr->getType())
        ));
    }

    Z3AstHandle visitFpToSigned(const ExprRef<FpToSignedExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_to_sbv(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            llvm::cast<BvType>(&expr->getType())->getWidth()
        ));
    }

    Z3AstHandle visitFpToUnsigned(const ExprRef<FpToUnsignedExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_to_ubv(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            llvm::cast<BvType>(&expr->getType())->getWidth()
        ));
    }

    // Floating-point arithmetic

    Z3AstHandle visitFAdd(const ExprRef<FAddExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_add(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            getOperand(1)
        ));
    }

    Z3AstHandle visitFSub(const ExprRef<FSubExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_sub(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            getOperand(1)
        ));
    }

    Z3AstHandle visitFMul(const ExprRef<FMulExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_mul(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            getOperand(1)
        ));
    }

    Z3AstHandle visitFDiv(const ExprRef<FDivExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_div(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            getOperand(0),
            getOperand(1)
        ));
    }

    Z3AstHandle visitFEq(const ExprRef<FEqExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_eq(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitFGt(const ExprRef<FGtExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_gt(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitFGtEq(const ExprRef<FGtEqExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_geq(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitFLt(const ExprRef<FLtExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_lt(mZ3Context, getOperand(0), getOperand(1)));
    }

    Z3AstHandle visitFLtEq(const ExprRef<FLtEqExpr>& expr)
    {
        return createHandle(Z3_mk_fpa_leq(mZ3Context, getOperand(0), getOperand(1)));
    }

    // Ternary
    Z3AstHandle visitSelect(const ExprRef<SelectExpr>& expr)
    {
        return createHandle(Z3_mk_ite(
            mZ3Context,
            getOperand(0), // condition
            getOperand(1), // then
            getOperand(2)  // else
        ));
    }

    // Arrays
    Z3AstHandle visitArrayRead(const ExprRef<ArrayReadExpr>& expr)
    {
        return createHandle(Z3_mk_select(
            mZ3Context,
            getOperand(0),
            getOperand(1)
        ));
    }

    Z3AstHandle visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr)
    {
        return createHandle(Z3_mk_store(
            mZ3Context,
            getOperand(0),
            getOperand(1),
            getOperand(2)
        ));
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
    Z3Solver::CacheMapT& mCache;
};

} // end anonymous namespace

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
    Z3ExprTransformer transformer(mZ3Context, mTmpCount, mCache);
    auto z3Expr = transformer.walk(expr);
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
    unsigned ebits = Z3_fpa_get_ebits(context, sort);
    unsigned sbits = Z3_fpa_get_sbits(context, sort);

    if (ebits == FloatType::ExpBitsInSingleTy && sbits == FloatType::SignificandBitsInSingleTy) {
        return FloatType::Single;
    } else if (ebits == FloatType::ExpBitsInDoubleTy && sbits == FloatType::SignificandBitsInDoubleTy) {
        return FloatType::Double;
    } else if (ebits == FloatType::ExpBitsInHalfTy && sbits == FloatType::SignificandBitsInHalfTy) {
        return FloatType::Half;
    } else if (ebits == FloatType::ExpBitsInQuadTy && sbits == FloatType::SignificandBitsInQuadTy) {
        return FloatType::Quad;
    }

    llvm_unreachable("Invalid floating point type");
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

    for (size_t i = 0; i < model.num_consts(); ++i) {
        z3::func_decl decl = model.get_const_decl(i);
        z3::expr z3Expr = model.get_const_interp(decl);

        auto name = decl.name().str();
        if (name.find("__gazer_undef") == 0) {
            continue;
        }

        auto variableOpt = mContext.getVariable(name);
        assert(variableOpt != nullptr && "The symbol table must contain a referenced variable.");

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
