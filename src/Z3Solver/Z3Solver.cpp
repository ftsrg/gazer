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

    virtual z3::expr visitLiteral(LiteralExpr& expr) override {
        if (expr.getType().isIntType()) {
            auto value = llvm::dyn_cast<IntLiteralExpr>(&expr)->getValue();
            return mContext.int_val(value);
        }

        llvm_unreachable("Unsupported operand type.");
    }

    virtual z3::expr visitVarRef(VarRefExpr& expr) override {
        if (expr.getType().isBoolType()) {
            return mContext.bool_const(expr.getVariable().getName().c_str());
        } else if (expr.getType().isIntType()) {
            return mContext.int_const(expr.getVariable().getName().c_str());
        }

        llvm_unreachable("Unsupported operand type.");
    }

    // Unary
    virtual z3::expr visitNot(NotExpr& expr) override {
        return !(visit(expr.getOperand()));
    }

    // Binary
    virtual z3::expr visitAdd(AddExpr& expr) override {
        return visit(expr.getLeft()) + visit(expr.getRight());
    }
    virtual z3::expr visitSub(SubExpr& expr) override {
        return visit(expr.getLeft()) - visit(expr.getRight());
    }
    virtual z3::expr visitMul(MulExpr& expr) override {
        return visit(expr.getLeft()) * visit(expr.getRight());
    }
    virtual z3::expr visitDiv(DivExpr& expr) override {
        return visit(expr.getLeft()) / visit(expr.getRight());}

    // Logic
    virtual z3::expr visitAnd(AndExpr& expr) override {
        return visit(expr.getLeft()) && visit(expr.getRight());
    }
    virtual z3::expr visitOr(OrExpr& expr) override {
        return visit(expr.getLeft()) || visit(expr.getRight());
    }
    virtual z3::expr visitXor(XorExpr& expr) override {
        return visit(expr.getLeft()) ^ visit(expr.getRight());
    }

    // Compare
    virtual z3::expr visitEq(EqExpr& expr) override {
        return visit(expr.getLeft()) == visit(expr.getRight());
    }
    virtual z3::expr visitNotEq(NotEqExpr& expr) override {
        return visit(expr.getLeft()) != visit(expr.getRight());
    }
    virtual z3::expr visitLt(LtExpr& expr) override {
        return visit(expr.getLeft()) < visit(expr.getRight());
    }
    virtual z3::expr visitLtEq(LtEqExpr& expr) override {
        return visit(expr.getLeft()) <= visit(expr.getRight());
    }
    virtual z3::expr visitGt(GtExpr& expr) override {
        return visit(expr.getLeft()) > visit(expr.getRight());
    }
    virtual z3::expr visitGtEq(GtEqExpr& expr) override {
        return visit(expr.getLeft()) >= visit(expr.getRight());
    }

    // Ternary
    virtual z3::expr visitSelect(SelectExpr& expr) override {
        return z3::ite(
            visit(expr.getCondition()),
            visit(expr.getThen()),
            visit(expr.getElse())
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
