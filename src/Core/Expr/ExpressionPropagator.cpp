/// \file This file defines the ExpressionPropagator class.
#include "gazer/Core/ExprVisitor.h"
#include "gazer/Core/Expr/ExprSimplify.h"
#include "gazer/Core/Expr/Matcher.h"

#include <llvm/ADT/DenseMap.h>

using namespace gazer;
using namespace gazer::PatternMatch;

namespace gazer
{

class ExpressionPropagator : public ExprVisitor<ExprPtr>
{
    struct PropagationTableEntry
    {
        ExprPtr expr;
        unsigned depth;

        PropagationTableEntry()
            : expr(nullptr), depth(~0)
        {}

        PropagationTableEntry(const ExprPtr &expr, unsigned depth)
            : expr(expr), depth(depth)
        {}
    };

    using PropagationTable = llvm::DenseMap<Variable*, PropagationTableEntry>;

public:
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

    ExprPtr combineEqualities(const ExprRef<NonNullaryExpr>& expr);

private:
    PropagationTable mPropTable;
    unsigned mDepthLimit;
    unsigned mCurrentDepth;
};

} // end namespace gazer

ExprPtr ExpressionPropagator::combineEqualities(const ExprRef<NonNullaryExpr> &expr)
{
    ExprRef<VarRefExpr> x1, x2;
    ExprRef<LiteralExpr> l1, l2;

    // NotEq(X1, L1) if Eq(X1, L2) where L1 != L2 --> True
    if (match(expr, m_NotEq(m_VarRef(x1), m_Literal(l1)))) {
        return BoolLiteralExpr::True(expr->getContext());
    }

    return expr;
}
