#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/ExprVisitor.h"

#include "gazer/ADT/ScopedCache.h"
#include "gazer/Support/Float.h"

#include <llvm/Support/raw_os_ostream.h>

#include <z3++.h>

using namespace gazer;

namespace
{

class Z3Solver : public Solver
{
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
};

class CachingZ3Solver final : public Z3Solver
{
public:
    using CacheMapT = ScopedCache<ExprPtr, Z3_ast, std::unordered_map<ExprPtr, Z3_ast>>;
    using Z3Solver::Z3Solver;

protected:
    void addConstraint(ExprPtr expr) override;
    void reset() override;

    void push() override;
    void pop() override;

private:
    CacheMapT mCache;
};

class Z3ExprTransformer : public ExprVisitor<z3::expr>
{
public:
    Z3ExprTransformer(z3::context& context, unsigned& tmpCount)
        : mZ3Context(context), mTmpCount(tmpCount)
    {}

protected:
    z3::sort typeToSort(const Type* type)
    {
        switch (type->getTypeID()) {
            case Type::BoolTypeID:
                return mZ3Context.bool_sort();
            case Type::IntTypeID:
                return mZ3Context.int_sort();
            case Type::BvTypeID: {
                auto intTy = llvm::cast<BvType>(type);
                return mZ3Context.bv_sort(intTy->getWidth());
            }
            case Type::FloatTypeID: {
                auto fltTy = llvm::cast<FloatType>(type);
                switch (fltTy->getPrecision()) {
                    case FloatType::Half:
                        return z3::sort(mZ3Context, Z3_mk_fpa_sort_half(mZ3Context));
                    case FloatType::Single:
                        return z3::sort(mZ3Context, Z3_mk_fpa_sort_single(mZ3Context));
                    case FloatType::Double:
                        return z3::sort(mZ3Context, Z3_mk_fpa_sort_double(mZ3Context));
                    case FloatType::Quad:
                        return z3::sort(mZ3Context, Z3_mk_fpa_sort_quadruple(mZ3Context));
                }
                llvm_unreachable("Invalid floating-point precision");
            }
            case Type::ArrayTypeID: {
                auto arrTy = llvm::cast<ArrayType>(type);
                return mZ3Context.array_sort(
                    typeToSort(&arrTy->getIndexType()),
                    typeToSort(&arrTy->getElementType())
                );
            }
        }

        assert(false && "Unsupported gazer type for Z3Solver");
    }

    z3::expr visitExpr(const ExprPtr& expr) override {
        assert(!"Unhandled expression type in Z3ExprTransformer.");
    }

    z3::expr visitUndef(const ExprRef<UndefExpr>& expr) override {
        std::string name = "__gazer_undef:" + std::to_string(mTmpCount++);

        return mZ3Context.constant(name.c_str(), typeToSort(&expr->getType()));
    }

    z3::expr visitLiteral(const ExprRef<LiteralExpr>& expr) override
    {
        if (expr->getType().isBvType()) {
            auto lit = llvm::dyn_cast<BvLiteralExpr>(&*expr);
            auto value = lit->getValue();

            return mZ3Context.bv_val(value.getLimitedValue(), lit->getType().getWidth());
        }
        
        if (expr->getType().isBoolType()) {
            auto value = llvm::dyn_cast<BoolLiteralExpr>(&*expr)->getValue();
            return mZ3Context.bool_val(value);
        }
        
        if (expr->getType().isIntType()) {
            int64_t value = llvm::dyn_cast<IntLiteralExpr>(&*expr)->getValue();
            return mZ3Context.int_val(value);    
        }
        
        if (expr->getType().isFloatType()) {
            auto fltTy = llvm::dyn_cast<FloatType>(&expr->getType());
            auto value = llvm::dyn_cast<FloatLiteralExpr>(&*expr)->getValue();
            
            if (fltTy->getPrecision() == FloatType::Single) {
                return z3::expr(mZ3Context, Z3_mk_fpa_numeral_float(
                    mZ3Context, value.convertToFloat(), typeToSort(fltTy)
                ));
            } else if (fltTy->getPrecision() == FloatType::Double) {
                return z3::expr(mZ3Context, Z3_mk_fpa_numeral_double(
                    mZ3Context, value.convertToDouble(), typeToSort(fltTy)
                ));
            }
        }

        assert(false && "Unsupported operand type.");
    }

    z3::expr visitVarRef(const ExprRef<VarRefExpr>& expr) override
    {
        auto name = expr->getVariable().getName();
        return mZ3Context.constant(name.c_str(), typeToSort(&expr->getType()));
    }

    // Unary
    z3::expr visitNot(const ExprRef<NotExpr>& expr) override {
        return !(visit(expr->getOperand()));
    }

    z3::expr visitZExt(const ExprRef<ZExtExpr>& expr) override {
        return z3::zext(visit(expr->getOperand()), expr->getWidthDiff());
    }
    z3::expr visitSExt(const ExprRef<SExtExpr>& expr) override {
        return z3::sext(visit(expr->getOperand()), expr->getWidthDiff());
    }
    z3::expr visitExtract(const ExprRef<ExtractExpr>& expr) override {
        unsigned hi = expr->getOffset() + expr->getWidth() - 1;
        unsigned lo = expr->getOffset();

        return visit(expr->getOperand()).extract(hi, lo);
    }

    // Binary
    z3::expr visitAdd(const ExprRef<AddExpr>& expr) override {
        return visit(expr->getLeft()) + visit(expr->getRight());
    }
    z3::expr visitSub(const ExprRef<SubExpr>& expr) override {
        return visit(expr->getLeft()) - visit(expr->getRight());
    }
    z3::expr visitMul(const ExprRef<MulExpr>& expr) override {
        return visit(expr->getLeft()) * visit(expr->getRight());
    }

    z3::expr visitSDiv(const ExprRef<SDivExpr>& expr) override {
        return visit(expr->getLeft()) / visit(expr->getRight());
    }
    z3::expr visitUDiv(const ExprRef<UDivExpr>& expr) override {
        return z3::udiv(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitSRem(const ExprRef<SRemExpr>& expr) override {
        return z3::srem(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitURem(const ExprRef<URemExpr>& expr) override {
        return z3::urem(visit(expr->getLeft()), visit(expr->getRight()));
    }

    z3::expr visitShl(const ExprRef<ShlExpr>& expr) override {
        return z3::shl(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitLShr(const ExprRef<LShrExpr>& expr) override {
        return z3::lshr(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitAShr(const ExprRef<AShrExpr>& expr) override {
        return z3::ashr(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitBAnd(const ExprRef<BAndExpr>& expr) override {
        return visit(expr->getLeft()) & visit(expr->getRight());
    }
    z3::expr visitBOr(const ExprRef<BOrExpr>& expr) override {
        return visit(expr->getLeft()) | visit(expr->getRight());
    }
    z3::expr visitBXor(const ExprRef<BXorExpr>& expr) override {
        return visit(expr->getLeft()) ^ visit(expr->getRight());
    }

    // Logic
    z3::expr visitAnd(const ExprRef<AndExpr>& expr) override {
        z3::expr_vector ops(mZ3Context);

        for (ExprPtr& op : expr->operands()) {
            ops.push_back(visit(op));
        }

        return z3::mk_and(ops);
    }

    z3::expr visitOr(const ExprRef<OrExpr>& expr) override {
        z3::expr_vector ops(mZ3Context);

        for (ExprPtr& op : expr->operands()) {
            ops.push_back(visit(op));
        }

        return z3::mk_or(ops);
    }

    z3::expr visitXor(const ExprRef<XorExpr>& expr) override {
        assert(expr->getType().isBoolType() && "Can only handle boolean XORs");

        return visit(expr->getLeft()) != visit(expr->getRight());
    }

    z3::expr visitImply(const ExprRef<ImplyExpr>& expr) override {
        assert(expr->getType().isBoolType() && "Can only handle boolean implications");

        return z3::implies(visit(expr->getLeft()), visit(expr->getRight()));
    }

    // Compare
    z3::expr visitEq(const ExprRef<EqExpr>& expr) override {
        return visit(expr->getLeft()) == visit(expr->getRight());
    }
    z3::expr visitNotEq(const ExprRef<NotEqExpr>& expr) override {
        return visit(expr->getLeft()) != visit(expr->getRight());
    }

    z3::expr visitSLt(const ExprRef<SLtExpr>& expr) override {
        return visit(expr->getLeft()) < visit(expr->getRight());
    }
    z3::expr visitSLtEq(const ExprRef<SLtEqExpr>& expr) override {
        return visit(expr->getLeft()) <= visit(expr->getRight());
    }
    z3::expr visitSGt(const ExprRef<SGtExpr>& expr) override {
        return visit(expr->getLeft()) > visit(expr->getRight());
    }
    z3::expr visitSGtEq(const ExprRef<SGtEqExpr>& expr) override {
        return visit(expr->getLeft()) >= visit(expr->getRight());
    }

    z3::expr visitULt(const ExprRef<ULtExpr>& expr) override {
        return z3::ult(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitULtEq(const ExprRef<ULtEqExpr>& expr) override {
        return z3::ule(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitUGt(const ExprRef<UGtExpr>& expr) override {
        return z3::ugt(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitUGtEq(const ExprRef<UGtEqExpr>& expr) override {
        return z3::uge(visit(expr->getLeft()), visit(expr->getRight()));
    }

    // Floating-point queries
    z3::expr visitFIsNan(const ExprRef<FIsNanExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_is_nan(mZ3Context, visit(expr->getOperand())));
    }
    z3::expr visitFIsInf(const ExprRef<FIsInfExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_is_infinite(mZ3Context, visit(expr->getOperand())));
    }

    // Floating-point casts
    z3::expr visitFCast(const ExprRef<FCastExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_to_fp_float(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getOperand()),
            typeToSort(&expr->getType())            
        ));
    }

    z3::expr visitSignedToFp(const ExprRef<SignedToFpExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_to_fp_signed(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getOperand()),
            typeToSort(&expr->getType())
        ));
    }

    z3::expr visitUnsignedToFp(const ExprRef<UnsignedToFpExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_to_fp_unsigned(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getOperand()),
            typeToSort(&expr->getType())
        ));
    }

    z3::expr visitFpToSigned(const ExprRef<FpToSignedExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_to_sbv(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getOperand()),
            llvm::cast<BvType>(&expr->getType())->getWidth()
        ));
    }

    z3::expr visitFpToUnsigned(const ExprRef<FpToUnsignedExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_to_ubv(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getOperand()),
            llvm::cast<BvType>(&expr->getType())->getWidth()
        ));
    }

    // Floating-point arithmetic
    z3::expr visitFAdd(const ExprRef<FAddExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_add(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getLeft()),
            visit(expr->getRight())
        ));
    }
    z3::expr visitFSub(const ExprRef<FSubExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_sub(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getLeft()),
            visit(expr->getRight())
        ));
    }
    z3::expr visitFMul(const ExprRef<FMulExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_mul(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getLeft()),
            visit(expr->getRight())
        ));
    }
    z3::expr visitFDiv(const ExprRef<FDivExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_div(
            mZ3Context,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getLeft()),
            visit(expr->getRight())
        ));
    }

    z3::expr visitFEq(const ExprRef<FEqExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_eq(
            mZ3Context, visit(expr->getLeft()), visit(expr->getRight())
        ));
    }
    z3::expr visitFGt(const ExprRef<FGtExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_gt(
            mZ3Context, visit(expr->getLeft()), visit(expr->getRight())
        ));
    }
    z3::expr visitFGtEq(const ExprRef<FGtEqExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_geq(
            mZ3Context, visit(expr->getLeft()), visit(expr->getRight())
        ));
    }
    z3::expr visitFLt(const ExprRef<FLtExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_lt(
            mZ3Context, visit(expr->getLeft()), visit(expr->getRight())
        ));
    }
    z3::expr visitFLtEq(const ExprRef<FLtEqExpr>& expr) override {
        return z3::expr(mZ3Context, Z3_mk_fpa_leq(
            mZ3Context, visit(expr->getLeft()), visit(expr->getRight())
        ));
    }

    // Ternary
    z3::expr visitSelect(const ExprRef<SelectExpr>& expr) override {
        return z3::ite(
            visit(expr->getCondition()),
            visit(expr->getThen()),
            visit(expr->getElse())
        );
    }

    // Arrays
    z3::expr visitArrayRead(const ExprRef<ArrayReadExpr>& expr) override {
        return z3::select(
            visit(expr->getArrayRef()),
            visit(expr->getIndex())
        );
    }

    z3::expr visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr) override {
        return z3::store(
            visit(expr->getArrayRef()),
            visit(expr->getIndex()),
            visit(expr->getElementValue())
        );
    }

protected:
    z3::expr transformRoundingMode(llvm::APFloat::roundingMode rm)
    {
        switch (rm) {
            case llvm::APFloat::roundingMode::rmNearestTiesToEven:
                return z3::expr(mZ3Context, Z3_mk_fpa_round_nearest_ties_to_even(mZ3Context));
            case llvm::APFloat::roundingMode::rmNearestTiesToAway:
                return z3::expr(mZ3Context, Z3_mk_fpa_round_nearest_ties_to_away(mZ3Context));
            case llvm::APFloat::roundingMode::rmTowardPositive:
                return z3::expr(mZ3Context, Z3_mk_fpa_round_toward_positive(mZ3Context));
            case llvm::APFloat::roundingMode::rmTowardNegative:
                return z3::expr(mZ3Context, Z3_mk_fpa_round_toward_negative(mZ3Context));
            case llvm::APFloat::roundingMode::rmTowardZero:
                return z3::expr(mZ3Context, Z3_mk_fpa_round_toward_zero(mZ3Context));
        }

        llvm_unreachable("Invalid rounding mode");
    }

protected:
    z3::context& mZ3Context;
    unsigned& mTmpCount;
};

class CachingZ3ExprTransformer : public Z3ExprTransformer
{
public:
    CachingZ3ExprTransformer(
        z3::context& context,
        unsigned& tmpCount,
        CachingZ3Solver::CacheMapT& cache)
        : Z3ExprTransformer(context, tmpCount), mCache(cache)   
    {}

    z3::expr visit(const ExprPtr& expr) override
    {
        if (expr->isNullary() || expr->isUnary()) {
            return ExprVisitor::visit(expr);
        }

        auto result = mCache.get(expr);
        if (result) {
            return z3::expr(mZ3Context, *result);
        }

        auto z3Expr = ExprVisitor::visit(expr);
        mCache.insert(expr, static_cast<Z3_ast>(z3Expr));

        return z3Expr;
    }
private:
    CachingZ3Solver::CacheMapT& mCache;
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
    Z3ExprTransformer transformer(mZ3Context, mTmpCount);
    auto z3Expr = transformer.visit(expr);
    mSolver.add(z3Expr);
}

void Z3Solver::reset()
{
    mSolver.reset();
}

void Z3Solver::push()
{
    mSolver.push();
}

void Z3Solver::pop()
{
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

void CachingZ3Solver::addConstraint(ExprPtr expr)
{
    CachingZ3ExprTransformer transformer(mZ3Context, mTmpCount, mCache);
    auto z3Expr = transformer.visit(expr);
    mSolver.add(z3Expr);
}

void CachingZ3Solver::reset()
{
    mCache.clear();
    Z3Solver::reset();
}

void CachingZ3Solver::push() {
    mCache.push();
    Z3Solver::push();
}

void CachingZ3Solver::pop() {
    mCache.pop();
    Z3Solver::pop();
}

//---- Support for model extraction ----//

static FloatType::FloatPrecision precFromSort(z3::context& context, z3::sort sort)
{
    assert(sort.sort_kind() == Z3_sort_kind::Z3_FLOATING_POINT_SORT);
    unsigned ebits = Z3_fpa_get_ebits(context, sort);
    unsigned sbits = Z3_fpa_get_sbits(context, sort);

    if (ebits == 8 && sbits == 24) {
        return FloatType::Single;
    } else if (ebits == 11 && sbits == 53) {
        return FloatType::Double;
    } else if (ebits == 5 && sbits == 11) {
        return FloatType::Half;
    } else if (ebits == 15 && sbits == 113) {
        return FloatType::Quad;
    }

    llvm_unreachable("Invalid floating point type");
}

llvm::APInt z3_bv_to_apint(z3::context& context, z3::model& model, const z3::expr& bv)
{
    assert(bv.is_bv() && "Bitvector conversion requires a bitvector");
    unsigned int width = Z3_get_bv_sort_size(context, bv.get_sort());

    if (width <= 64) {
        uint64_t value;
        Z3_get_numeral_uint64(context, bv, &value);

        return llvm::APInt(width, value);
    } else {
        llvm::SmallVector<uint64_t, 2> bits;
        for (size_t i = 0; i < width; i += 64) {
            uint64_t value;
            Z3_get_numeral_uint64(context, model.eval(bv.extract(i, 64)), &value);

            bits.push_back(value);
        }

        return llvm::APInt(width, bits);
    }
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
        
            auto intTy = llvm::cast<gazer::IntType>(varTy);

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
            assert(false && "Unhandled Z3 expression type.");
        }

        builder.put(&variable, expr);
    }

    return builder.build();
}

std::unique_ptr<Solver> Z3SolverFactory::createSolver(GazerContext& context)
{
    if (mCache) {
        return std::unique_ptr<Solver>(new CachingZ3Solver(context));
    }

    return std::unique_ptr<Solver>(new Z3Solver(context));
}
