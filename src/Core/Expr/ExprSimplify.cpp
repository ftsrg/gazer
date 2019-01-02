#include "gazer/Core/ExprVisitor.h"
#include "gazer/Core/Valuation.h"
#include "gazer/Core/Expr/Matcher.h"

#include <llvm/ADT/DenseMap.h>
#include <stack>

using namespace gazer;
using namespace gazer::PatternMatch;

namespace
{

struct Fact
{
    const Variable* variable;
    ExprRef<LiteralExpr> litVal;
};

class ExprSimplifyVisitor final : public ExprVisitor<ExprPtr>
{
public:
    ExprPtr visitExpr(const ExprPtr& expr) override { return expr; }

    ExprPtr visitVarRef(const std::shared_ptr<VarRefExpr>& expr) {
        if
    }

    ExprPtr visitNonNullary(const std::shared_ptr<NonNullaryExpr>& expr)
    {
        ExprVector ops;
        for (size_t i = 0; i < expr->getNumOperands(); ++i) {
            auto newOp = this->visit(expr->getOperand(i));
            ops.push_back(newOp);
        }

        if (std::equal(expr->op_begin(), expr->op_end(), ops.begin())) {
            // No changes were done, just return the original expression
            return expr;
        }

        return NonNullaryExpr::clone(expr, ops);
    }

    ExprPtr visitAnd(const std::shared_ptr<AndExpr>& expr)
    {
        // Push all 'simple' fact expressions onto a new fact scope
        mScopes.push(mFacts.size());

        for (const ExprPtr& operand : expr->operands()) {
            ExprRef<VarRefExpr> varRef;
            ExprRef<LiteralExpr> literal;

            if (match(operand, m_Eq(m_VarRef(varRef), m_Literal(literal)))) {
                mFacts.push_back(Fact{&varRef->getVariable(), literal});
            }
        }

        ExprPtr result = this->visitNonNullary(expr);

        size_t newSize = mScopes.top();
        mFacts.resize(newSize);
        mScopes.pop();

        return result;
    }

private:
    std::vector<Fact> mFacts;
    std::stack<size_t> mScopes;
};

}
