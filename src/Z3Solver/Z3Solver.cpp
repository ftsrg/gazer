#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include "gazer/Core/ExprVisitor.h"

#include <unordered_map>

#include <llvm/Support/raw_os_ostream.h>

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
    virtual z3::expr visitExpr(const ExprPtr& expr) override {
        throw std::logic_error("Unhandled expression type in Z3ExprTransformer.");
    }

    virtual z3::expr visitUndef(const std::shared_ptr<UndefExpr>& expr) override {
        std::string name = "__gazer_undef:" + std::to_string(mTmpCount++);

        if (expr->getType().isBoolType()) {
            return mContext.bool_const(name.c_str());
        } else if (expr->getType().isIntType()) {
            auto intType = llvm::dyn_cast<IntType>(&expr->getType());
            return mContext.bv_const(
                name.c_str(),
                intType->getWidth()
            );
        }

        assert(false && "Unsupported operand type.");
    }

    virtual z3::expr visitLiteral(const std::shared_ptr<LiteralExpr>& expr) override {
        if (expr->getType().isIntType()) {
            auto lit = llvm::dyn_cast<IntLiteralExpr>(&*expr);
            auto value = lit->getValue();
            return mContext.bv_val(
                static_cast<__uint64>(value.getLimitedValue()),
                lit->getType().getWidth()
            );
        } else if (expr->getType().isBoolType()) {
            auto value = llvm::dyn_cast<BoolLiteralExpr>(&*expr)->getValue();
            return mContext.bool_val(value);
        }

        assert(false && "Unsupported operand type.");
    }

    virtual z3::expr visitVarRef(const std::shared_ptr<VarRefExpr>& expr) override {
        if (expr->getType().isBoolType()) {
            return mContext.bool_const(expr->getVariable().getName().c_str());
        } else if (expr->getType().isIntType()) {
            auto intType = llvm::dyn_cast<IntType>(&expr->getType());
            return mContext.bv_const(
                expr->getVariable().getName().c_str(),
                intType->getWidth()
            );
        }

        assert(false && "Unsupported operand type.");
    }

    // Unary
    virtual z3::expr visitNot(const std::shared_ptr<NotExpr>& expr) override {
        return !(visit(expr->getOperand()));
    }

    virtual z3::expr visitZExt(const std::shared_ptr<ZExtExpr>& expr) override {
        return z3::zext(visit(expr->getOperand()), expr->getWidthDiff());
    }
    virtual z3::expr visitSExt(const std::shared_ptr<SExtExpr>& expr) override {
        return z3::sext(visit(expr->getOperand()), expr->getWidthDiff());
    }
    virtual z3::expr visitTrunc(const std::shared_ptr<TruncExpr>& expr) override {
        return visit(expr->getOperand()).extract(expr->getTruncatedWidth() - 1, 0);
    }

    // Binary
    virtual z3::expr visitAdd(const std::shared_ptr<AddExpr>& expr) override {
        return visit(expr->getLeft()) + visit(expr->getRight());
    }
    virtual z3::expr visitSub(const std::shared_ptr<SubExpr>& expr) override {
        return visit(expr->getLeft()) - visit(expr->getRight());
    }
    virtual z3::expr visitMul(const std::shared_ptr<MulExpr>& expr) override {
        return visit(expr->getLeft()) * visit(expr->getRight());
    }
    virtual z3::expr visitDiv(const std::shared_ptr<DivExpr>& expr) override {
        return visit(expr->getLeft()) / visit(expr->getRight());
    }

    virtual z3::expr visitShl(const std::shared_ptr<ShlExpr>& expr) override {
        return z3::shl(visit(expr->getLeft()), visit(expr->getRight()));
    }
    virtual z3::expr visitLShr(const std::shared_ptr<LShrExpr>& expr) override {
        return z3::lshr(visit(expr->getLeft()), visit(expr->getRight()));
    }
    virtual z3::expr visitAShr(const std::shared_ptr<AShrExpr>& expr) override {
        return z3::ashr(visit(expr->getLeft()), visit(expr->getRight()));
    }
    virtual z3::expr visitBAnd(const std::shared_ptr<BAndExpr>& expr) override {
        return visit(expr->getLeft()) & visit(expr->getRight());
    }
    virtual z3::expr visitBOr(const std::shared_ptr<BOrExpr>& expr) override {
        return visit(expr->getLeft()) | visit(expr->getRight());
    }
    virtual z3::expr visitBXor(const std::shared_ptr<BXorExpr>& expr) override {
        return visit(expr->getLeft()) ^ visit(expr->getRight());
    }

    // Logic
    virtual z3::expr visitAnd(const std::shared_ptr<AndExpr>& expr) override {
        z3::expr_vector ops(mContext);

        for (ExprPtr& op : expr->operands()) {
            ops.push_back(visit(op));
        }

        return z3::mk_and(ops);
    }
    virtual z3::expr visitOr(const std::shared_ptr<OrExpr>& expr) override {
        z3::expr_vector ops(mContext);

        for (ExprPtr& op : expr->operands()) {
            ops.push_back(visit(op));
        }

        return z3::mk_or(ops);
    }
    virtual z3::expr visitXor(const std::shared_ptr<XorExpr>& expr) override {
        if (expr->getType().isBoolType()) {
            return visit(expr->getLeft()) != visit(expr->getRight());
        }
        assert(false && "Can only handle boolean XORs");
    }

    // Compare
    virtual z3::expr visitEq(const std::shared_ptr<EqExpr>& expr) override {
        return visit(expr->getLeft()) == visit(expr->getRight());
    }
    virtual z3::expr visitNotEq(const std::shared_ptr<NotEqExpr>& expr) override {
        return visit(expr->getLeft()) != visit(expr->getRight());
    }

    virtual z3::expr visitSLt(const std::shared_ptr<SLtExpr>& expr) override {
        return visit(expr->getLeft()) < visit(expr->getRight());
    }
    virtual z3::expr visitSLtEq(const std::shared_ptr<SLtEqExpr>& expr) override {
        return visit(expr->getLeft()) <= visit(expr->getRight());
    }
    virtual z3::expr visitSGt(const std::shared_ptr<SGtExpr>& expr) override {
        return visit(expr->getLeft()) > visit(expr->getRight());
    }
    virtual z3::expr visitSGtEq(const std::shared_ptr<SGtEqExpr>& expr) override {
        return visit(expr->getLeft()) >= visit(expr->getRight());
    }

    virtual z3::expr visitULt(const std::shared_ptr<ULtExpr>& expr) override {
        return z3::ult(visit(expr->getLeft()), visit(expr->getRight()));
    }
    virtual z3::expr visitULtEq(const std::shared_ptr<ULtEqExpr>& expr) override {
        return z3::ule(visit(expr->getLeft()), visit(expr->getRight()));
    }
    virtual z3::expr visitUGt(const std::shared_ptr<UGtExpr>& expr) override {
        return z3::ugt(visit(expr->getLeft()), visit(expr->getRight()));
    }
    virtual z3::expr visitUGtEq(const std::shared_ptr<UGtEqExpr>& expr) override {
        return z3::uge(visit(expr->getLeft()), visit(expr->getRight()));
    }

    // Ternary
    virtual z3::expr visitSelect(const std::shared_ptr<SelectExpr>& expr) override {
        return z3::ite(
            visit(expr->getCondition()),
            visit(expr->getThen()),
            visit(expr->getElse())
        );
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
    Z3ExprTransformer transformer(mContext, mTmpCount);
    auto z3Expr = transformer.visit(expr);
    mSolver.add(z3Expr);
}

void CachingZ3Solver::addConstraint(ExprPtr expr)
{
    CachingZ3ExprTransformer transformer(mContext, mTmpCount, mCache);
    auto z3Expr = transformer.visit(expr);
    mSolver.add(z3Expr);
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
            unsigned long long value;
            Z3_get_numeral_uint64(mContext, z3Expr, &value);

            auto iVal = llvm::APInt(width, value);
            expr = IntLiteralExpr::get(
                *IntType::get(width),
                iVal
            );
        } else {
            assert(false && "Unhandled Z3 expression type.");
        }

        builder.put(&variable, expr);
    }

    return builder.build();
}
