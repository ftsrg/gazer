/// \file This file defines the ExpressionPropagator class.
#include "gazer/Core/ExprVisitor.h"
#include "gazer/Core/Expr/Matcher.h"
#include "gazer/Core/Expr/ExprPropagator.h"

#include <llvm/ADT/DenseMap.h>

using namespace gazer;
using namespace gazer::PatternMatch;

namespace gazer
{

class ExpressionPropagator : public ExprVisitor<ExprPtr>
{
public:
    ExpressionPropagator(PropagationTable& propTable, unsigned maxDepth)
        : mPropTable(propTable), mDepthLimit(maxDepth), mCurrentDepth(0)
    {}

    ExprPtr visit(const ExprPtr &expr) override {
        if (mCurrentDepth > mDepthLimit) {
            return expr;
        }

        return ExprVisitor::visit(expr);
    }

protected:
    ExprPtr visitExpr(const ExprPtr &expr) override {
        return expr;
    }

    ExprPtr visitNonNullary(const ExprRef<NonNullaryExpr>& expr) {
        ExprVector ops;
        ops.reserve(expr->getNumOperands());

        ++mCurrentDepth;
        for (auto& operand : expr->operands()) {
            auto newOp = this->visit(operand);
            ops.push_back(newOp);
        }
        --mCurrentDepth;

        return expr->clone(ops);
    }

    ExprPtr visitVarRef(const ExprRef<VarRefExpr>& expr) {
        Variable* variable = &expr->getVariable();

        auto range = mPropTable.get(variable);


        return expr;
    }

private:
    PropagationTable& mPropTable;
    unsigned mDepthLimit;
    unsigned mCurrentDepth;
};

ExprPtr PropagateExpression(ExprPtr expr, PropagationTable& propTable, unsigned depth)
{
    ExpressionPropagator propagator(propTable, depth);

    return propagator.visit(expr);
}

} // end namespace gazer
