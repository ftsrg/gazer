#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include "gazer/Core/ExprVisitor.h"
#include "gazer/Support/Float.h"

#include <llvm/Support/raw_os_ostream.h>

#include <unordered_map>

using namespace gazer;

namespace
{

// Z3 

class Z3ExprTransformer : public ExprVisitor<z3::expr>
{
public:
    Z3ExprTransformer(z3::context& context, unsigned& tmpCount)
        : mContext(context), mTmpCount(tmpCount)
    {}

protected:
    z3::sort typeToSort(const Type* type)
    {
        switch (type->getTypeID()) {
            case Type::BoolTypeID:
                return mContext.bool_sort();
            case Type::MathIntTypeID:
                return mContext.int_sort();
            case Type::IntTypeID: {
                auto intTy = llvm::cast<IntType>(type);
                return mContext.bv_sort(intTy->getWidth());
            }
            case Type::FloatTypeID: {
                auto fltTy = llvm::cast<FloatType>(type);
                switch (fltTy->getPrecision()) {
                    case FloatType::Half:
                        return z3::sort(mContext, Z3_mk_fpa_sort_half(mContext));
                    case FloatType::Single:
                        return z3::sort(mContext, Z3_mk_fpa_sort_single(mContext));
                    case FloatType::Double:
                        return z3::sort(mContext, Z3_mk_fpa_sort_double(mContext));
                    case FloatType::Quad:
                        return z3::sort(mContext, Z3_mk_fpa_sort_quadruple(mContext));
                }
                llvm_unreachable("Invalid floating-point precision");
            }
            case Type::ArrayTypeID: {
                auto arrTy = llvm::cast<ArrayType>(type);
                return mContext.array_sort(
                    typeToSort(arrTy->getIndexType()),
                    typeToSort(arrTy->getElementType())
                );
            }
            case Type::PointerTypeID: {
                // Pointers are represented with integers
                return mContext.int_sort();
            }
        }

        assert(false && "Unsupported gazer type for Z3Solver");
    }

    z3::expr visitExpr(const ExprPtr& expr) override {
        throw std::logic_error("Unhandled expression type in Z3ExprTransformer.");
    }

    z3::expr visitUndef(const std::shared_ptr<UndefExpr>& expr) override {
        std::string name = "__gazer_undef:" + std::to_string(mTmpCount++);

        return mContext.constant(name.c_str(), typeToSort(&expr->getType()));
    }

    z3::expr visitLiteral(const std::shared_ptr<LiteralExpr>& expr) override {
        if (expr->getType().isIntType()) {
            auto lit = llvm::dyn_cast<IntLiteralExpr>(&*expr);
            auto value = lit->getValue();
            return mContext.bv_val(
                value.getLimitedValue(),
                lit->getType().getWidth()
            );
        } else if (expr->getType().isBoolType()) {
            auto value = llvm::dyn_cast<BoolLiteralExpr>(&*expr)->getValue();
            return mContext.bool_val(value);
        } else if (expr->getType().isMathIntType()) {
            int64_t value = llvm::dyn_cast<MathIntLiteralExpr>(&*expr)->getValue();
            return mContext.int_val(value);    
        } else if (expr->getType().isFloatType()) {
            auto fltTy = llvm::dyn_cast<FloatType>(&expr->getType());
            auto value = llvm::dyn_cast<FloatLiteralExpr>(&*expr)->getValue();
            
            if (fltTy->getPrecision() == FloatType::Single) {
                return z3::expr(mContext,
                    Z3_mk_fpa_numeral_float(
                        mContext, value.convertToFloat(), typeToSort(fltTy)
                    )
                );
            } else if (fltTy->getPrecision() == FloatType::Double) {
                return z3::expr(mContext,
                    Z3_mk_fpa_numeral_double(
                        mContext, value.convertToDouble(), typeToSort(fltTy)
                    )
                );
            }
        }

        assert(false && "Unsupported operand type.");
    }

    z3::expr visitVarRef(const std::shared_ptr<VarRefExpr>& expr) override {
        auto name = expr->getVariable().getName();

        return mContext.constant(name.c_str(), typeToSort(&expr->getType()));
    }

    // Unary
    z3::expr visitNot(const std::shared_ptr<NotExpr>& expr) override {
        return !(visit(expr->getOperand()));
    }

    z3::expr visitZExt(const std::shared_ptr<ZExtExpr>& expr) override {
        return z3::zext(visit(expr->getOperand()), expr->getWidthDiff());
    }
    z3::expr visitSExt(const std::shared_ptr<SExtExpr>& expr) override {
        return z3::sext(visit(expr->getOperand()), expr->getWidthDiff());
    }
    z3::expr visitExtract(const std::shared_ptr<ExtractExpr>& expr) override {
        unsigned hi = expr->getOffset() + expr->getWidth() - 1;
        unsigned lo = expr->getOffset();

        return visit(expr->getOperand()).extract(hi, lo);
    }

    // Binary
    z3::expr visitAdd(const std::shared_ptr<AddExpr>& expr) override {
        return visit(expr->getLeft()) + visit(expr->getRight());
    }
    z3::expr visitSub(const std::shared_ptr<SubExpr>& expr) override {
        return visit(expr->getLeft()) - visit(expr->getRight());
    }
    z3::expr visitMul(const std::shared_ptr<MulExpr>& expr) override {
        return visit(expr->getLeft()) * visit(expr->getRight());
    }

    z3::expr visitSDiv(const std::shared_ptr<SDivExpr>& expr) override {
        return visit(expr->getLeft()) / visit(expr->getRight());
    }
    z3::expr visitUDiv(const std::shared_ptr<UDivExpr>& expr) override {
        return z3::udiv(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitSRem(const std::shared_ptr<SRemExpr>& expr) override {
        return z3::srem(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitURem(const std::shared_ptr<URemExpr>& expr) override {
        return z3::urem(visit(expr->getLeft()), visit(expr->getRight()));
    }

    z3::expr visitShl(const std::shared_ptr<ShlExpr>& expr) override {
        return z3::shl(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitLShr(const std::shared_ptr<LShrExpr>& expr) override {
        return z3::lshr(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitAShr(const std::shared_ptr<AShrExpr>& expr) override {
        return z3::ashr(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitBAnd(const std::shared_ptr<BAndExpr>& expr) override {
        return visit(expr->getLeft()) & visit(expr->getRight());
    }
    z3::expr visitBOr(const std::shared_ptr<BOrExpr>& expr) override {
        return visit(expr->getLeft()) | visit(expr->getRight());
    }
    z3::expr visitBXor(const std::shared_ptr<BXorExpr>& expr) override {
        return visit(expr->getLeft()) ^ visit(expr->getRight());
    }

    // Logic
    z3::expr visitAnd(const std::shared_ptr<AndExpr>& expr) override {
        z3::expr_vector ops(mContext);

        for (ExprPtr& op : expr->operands()) {
            ops.push_back(visit(op));
        }

        return z3::mk_and(ops);
    }
    z3::expr visitOr(const std::shared_ptr<OrExpr>& expr) override {
        z3::expr_vector ops(mContext);

        for (ExprPtr& op : expr->operands()) {
            ops.push_back(visit(op));
        }

        return z3::mk_or(ops);
    }
    z3::expr visitXor(const std::shared_ptr<XorExpr>& expr) override {
        if (expr->getType().isBoolType()) {
            return visit(expr->getLeft()) != visit(expr->getRight());
        }
        assert(false && "Can only handle boolean XORs");
    }

    // Compare
    z3::expr visitEq(const std::shared_ptr<EqExpr>& expr) override {
        return visit(expr->getLeft()) == visit(expr->getRight());
    }
    z3::expr visitNotEq(const std::shared_ptr<NotEqExpr>& expr) override {
        return visit(expr->getLeft()) != visit(expr->getRight());
    }

    z3::expr visitSLt(const std::shared_ptr<SLtExpr>& expr) override {
        return visit(expr->getLeft()) < visit(expr->getRight());
    }
    z3::expr visitSLtEq(const std::shared_ptr<SLtEqExpr>& expr) override {
        return visit(expr->getLeft()) <= visit(expr->getRight());
    }
    z3::expr visitSGt(const std::shared_ptr<SGtExpr>& expr) override {
        return visit(expr->getLeft()) > visit(expr->getRight());
    }
    z3::expr visitSGtEq(const std::shared_ptr<SGtEqExpr>& expr) override {
        return visit(expr->getLeft()) >= visit(expr->getRight());
    }

    z3::expr visitULt(const std::shared_ptr<ULtExpr>& expr) override {
        return z3::ult(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitULtEq(const std::shared_ptr<ULtEqExpr>& expr) override {
        return z3::ule(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitUGt(const std::shared_ptr<UGtExpr>& expr) override {
        return z3::ugt(visit(expr->getLeft()), visit(expr->getRight()));
    }
    z3::expr visitUGtEq(const std::shared_ptr<UGtEqExpr>& expr) override {
        return z3::uge(visit(expr->getLeft()), visit(expr->getRight()));
    }

    // Floating-point queries
    z3::expr visitFIsNan(const std::shared_ptr<FIsNanExpr>& expr) override {
        return z3::expr(mContext, Z3_mk_fpa_is_nan(mContext, visit(expr->getOperand())));
    }
    z3::expr visitFIsInf(const std::shared_ptr<FIsInfExpr>& expr) override {
        return z3::expr(mContext, Z3_mk_fpa_is_infinite(mContext, visit(expr->getOperand())));
    }

    // Floating-point arithmetic
    z3::expr visitFAdd(const std::shared_ptr<FAddExpr>& expr) override {
        return z3::expr(mContext, Z3_mk_fpa_add(
            mContext,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getLeft()),
            visit(expr->getRight())
        ));
    }
    z3::expr visitFSub(const std::shared_ptr<FSubExpr>& expr) override {
        return z3::expr(mContext, Z3_mk_fpa_sub(
            mContext,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getLeft()),
            visit(expr->getRight())
        ));
    }
    z3::expr visitFMul(const std::shared_ptr<FMulExpr>& expr) override {
        return z3::expr(mContext, Z3_mk_fpa_mul(
            mContext,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getLeft()),
            visit(expr->getRight())
        ));
    }
    z3::expr visitFDiv(const std::shared_ptr<FDivExpr>& expr) override {
        return z3::expr(mContext, Z3_mk_fpa_div(
            mContext,
            transformRoundingMode(expr->getRoundingMode()),
            visit(expr->getLeft()),
            visit(expr->getRight())
        ));
    }

    z3::expr visitFEq(const std::shared_ptr<FEqExpr>& expr) override {
        return z3::expr(mContext, Z3_mk_fpa_eq(
            mContext, visit(expr->getLeft()), visit(expr->getRight())
        ));
    }
    z3::expr visitFGt(const std::shared_ptr<FGtExpr>& expr) override {
        return z3::expr(mContext, Z3_mk_fpa_gt(
            mContext, visit(expr->getLeft()), visit(expr->getRight())
        ));
    }
    z3::expr visitFGtEq(const std::shared_ptr<FGtEqExpr>& expr) override {
        return z3::expr(mContext, Z3_mk_fpa_geq(
            mContext, visit(expr->getLeft()), visit(expr->getRight())
        ));
    }
    z3::expr visitFLt(const std::shared_ptr<FLtExpr>& expr) override {
        return z3::expr(mContext, Z3_mk_fpa_lt(
            mContext, visit(expr->getLeft()), visit(expr->getRight())
        ));
    }
    z3::expr visitFLtEq(const std::shared_ptr<FLtEqExpr>& expr) override {
        return z3::expr(mContext, Z3_mk_fpa_leq(
            mContext, visit(expr->getLeft()), visit(expr->getRight())
        ));
    }

    // Ternary
    z3::expr visitSelect(const std::shared_ptr<SelectExpr>& expr) override {
        return z3::ite(
            visit(expr->getCondition()),
            visit(expr->getThen()),
            visit(expr->getElse())
        );
    }

    // Arrays
    z3::expr visitArrayRead(const std::shared_ptr<ArrayReadExpr>& expr) override {
        return z3::select(
            visit(expr->getArrayRef()),
            visit(expr->getIndex())
        );
    }

    z3::expr visitArrayWrite(const std::shared_ptr<ArrayWriteExpr>& expr) override {
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
                return z3::expr(mContext, Z3_mk_fpa_round_nearest_ties_to_even(mContext));
            case llvm::APFloat::roundingMode::rmNearestTiesToAway:
                return z3::expr(mContext, Z3_mk_fpa_round_nearest_ties_to_away(mContext));
            case llvm::APFloat::roundingMode::rmTowardPositive:
                return z3::expr(mContext, Z3_mk_fpa_round_toward_positive(mContext));
            case llvm::APFloat::roundingMode::rmTowardNegative:
                return z3::expr(mContext, Z3_mk_fpa_round_toward_negative(mContext));
            case llvm::APFloat::roundingMode::rmTowardZero:
                return z3::expr(mContext, Z3_mk_fpa_round_toward_zero(mContext));
        }

        llvm_unreachable("Invalid rounding mode");
    }

protected:
    z3::context& mContext;
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

    z3::expr visit(const ExprPtr& expr) override {
        if (expr->isNullary() || expr->isUnary()) {
            return ExprVisitor::visit(expr);
        }

        auto result = mCache.find(expr.get());
        if (result != mCache.end()) {
            return z3::expr(mContext, result->second);
        }

        auto z3Expr = ExprVisitor::visit(expr);

        mCache[expr.get()] = (Z3_ast) z3Expr;

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
    Z3ExprTransformer transformer(mContext, mTmpCount);
    auto z3Expr = transformer.visit(expr);
    mSolver.add(z3Expr);
}

void Z3Solver::dump(llvm::raw_ostream& os)
{
    os << Z3_solver_to_string(mContext, mSolver);    
}

void CachingZ3Solver::addConstraint(ExprPtr expr)
{
    CachingZ3ExprTransformer transformer(mContext, mTmpCount, mCache);
    auto z3Expr = transformer.visit(expr);
    mSolver.add(z3Expr);
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

llvm::APInt z3_bv_to_apint(z3::context& context, z3::model& model, z3::expr bv)
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

        auto variableOpt = mSymbols.get(name);
        assert(variableOpt.has_value() && "The symbol table must contain a referenced variable.");

        Variable& variable = variableOpt->get();
        std::shared_ptr<LiteralExpr> expr = nullptr;

        if (z3Expr.is_bool()) {
            bool value = z3::eq(model.eval(z3Expr), mContext.bool_val(true));
            expr = BoolLiteralExpr::Get(value);            
        } else if (z3Expr.is_bv()) {
            unsigned int width = Z3_get_bv_sort_size(mContext, z3Expr.get_sort());
            uint64_t value;
            Z3_get_numeral_uint64(mContext, z3Expr, &value);

            llvm::APInt iVal(width, value);
            expr = IntLiteralExpr::get(
                *IntType::get(width),
                iVal
            );
        } else if (z3Expr.get_sort().sort_kind() == Z3_sort_kind::Z3_FLOATING_POINT_SORT) {
            z3::sort sort = z3Expr.get_sort();
            FloatType::FloatPrecision precision = precFromSort(mContext, sort);
            auto& fltTy = *FloatType::get(precision);

            bool isNaN = z3::eq(
                model.eval(z3::expr(mContext, Z3_mk_fpa_is_nan(mContext, z3Expr))),
                mContext.bool_val(true)
            );

            if (isNaN) {
                expr = FloatLiteralExpr::get(fltTy, llvm::APFloat::getNaN(
                    fltTy.getLLVMSemantics()
                ));
            } else {
                auto toIEEE = z3::expr(mContext, Z3_mk_fpa_to_ieee_bv(mContext, z3Expr));
                auto ieeeVal = model.eval(toIEEE);

                uint64_t bits;
                Z3_get_numeral_uint64(mContext,  ieeeVal, &bits);

                llvm::APInt bv(fltTy.getWidth(), bits);
                llvm::APFloat apflt(fltTy.getLLVMSemantics(), bv);

                expr = FloatLiteralExpr::get(fltTy, apflt);
            }

        } else {
            assert(false && "Unhandled Z3 expression type.");
        }

        builder.put(&variable, expr);
    }

    return builder.build();
}

std::unique_ptr<Solver> Z3SolverFactory::createSolver(SymbolTable& symbols)
{
    if (mCache) {
        return std::unique_ptr<Solver>(new CachingZ3Solver(symbols));
    }

    return std::unique_ptr<Solver>(new Z3Solver(symbols));
}
