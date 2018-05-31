#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include "gazer/Core/ExprVisitor.h"

using namespace gazer;

namespace
{

class Z3ExprTransformer : public ExprVisitor<z3::expr>
{
public:
    Z3ExprTransformer(z3::context& context)
        : mContext(context)
    {}

protected:
    virtual z3::expr visitExpr(const ExprPtr& expr) override {
        throw std::logic_error("Unhandled expression type in Z3ExprTransformer.");
    }

    virtual z3::expr visitLiteral(const std::shared_ptr<LiteralExpr>& expr) override {
        if (expr->getType().isIntType()) {
            auto lit = llvm::dyn_cast<IntLiteralExpr>(&*expr);
            auto value = lit->getValue();
            return mContext.bv_val(
                static_cast<__uint64>(value),
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
        return visit(expr->getLeft()) / visit(expr->getRight());}

    // Logic
    virtual z3::expr visitAnd(const std::shared_ptr<AndExpr>& expr) override {
        return visit(expr->getLeft()) && visit(expr->getRight());
    }
    virtual z3::expr visitOr(const std::shared_ptr<OrExpr>& expr) override {
        return visit(expr->getLeft()) || visit(expr->getRight());
    }
    virtual z3::expr visitXor(const std::shared_ptr<XorExpr>& expr) override {
        return visit(expr->getLeft()) ^ visit(expr->getRight());
    }

    // Compare
    virtual z3::expr visitEq(const std::shared_ptr<EqExpr>& expr) override {
        return visit(expr->getLeft()) == visit(expr->getRight());
    }
    virtual z3::expr visitNotEq(const std::shared_ptr<NotEqExpr>& expr) override {
        return visit(expr->getLeft()) != visit(expr->getRight());
    }
    virtual z3::expr visitLt(const std::shared_ptr<LtExpr>& expr) override {
        return visit(expr->getLeft()) < visit(expr->getRight());
    }
    virtual z3::expr visitLtEq(const std::shared_ptr<LtEqExpr>& expr) override {
        return visit(expr->getLeft()) <= visit(expr->getRight());
    }
    virtual z3::expr visitGt(const std::shared_ptr<GtExpr>& expr) override {
        return visit(expr->getLeft()) > visit(expr->getRight());
    }
    virtual z3::expr visitGtEq(const std::shared_ptr<GtEqExpr>& expr) override {
        return visit(expr->getLeft()) >= visit(expr->getRight());
    }

    // Ternary
    virtual z3::expr visitSelect(const std::shared_ptr<SelectExpr>& expr) override {
        return z3::ite(
            visit(expr->getCondition()),
            visit(expr->getThen()),
            visit(expr->getElse())
        );
    }
private:
    z3::context& mContext;
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
    Z3ExprTransformer transformer(mContext);
    mSolver.add(transformer.visit(expr));
}
