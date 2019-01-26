#include "gazer/Core/Expr/ExprSimplify.h"
#include "gazer/Core/ExprVisitor.h"
#include "gazer/Core/Expr/Matcher.h"
#include "gazer/Core/Expr/ConstantFolder.h"
#include "gazer/ADT/ScopedMap.h"
#include "gazer/ADT/ScopedVector.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/raw_ostream.h>

#include <stack>

using namespace gazer;
using namespace gazer::PatternMatch;

namespace
{

struct LiteralValue
{
    ExprRef<VarRefExpr> variable;
    ExprRef<LiteralExpr> litVal;

    LiteralValue(ExprRef<VarRefExpr> variable, ExprRef<gazer::LiteralExpr> litVal)
        : variable(variable), litVal(litVal)
    {}
};

class ExprSimplifyVisitor final : public ExprVisitor<ExprPtr>
{
public:
    ExprSimplifyVisitor(ExprSimplifier::OptLevel level)
        : mLevel(level)
    {}

    ExprPtr visit(const ExprPtr& expr) override
    {
        ++mCurrentDepth;
        ExprPtr result = nullptr;
        if (expr->isNullary() || expr->isUnary()) {
            result = ExprVisitor::visit(expr);
        } else {
            auto cached = mVisited.find(expr.get());
            if (cached != mVisited.end()) {
                result = cached->second;
            } else {
                result = ExprVisitor::visit(expr);
                if (this->isSafeToCache()) {
                    mVisited[expr.get()] = result;
                }
            }
        }

        --mCurrentDepth;
        return result;
    }

protected:
    ExprPtr visitExpr(const ExprPtr& expr) override { return expr; }

    ExprPtr visitVarRef(const ExprRef<VarRefExpr>& expr) override
    {
        //const Variable* variable = &expr->getVariable();
        //if (mLiteralValues.count(variable) != 0) {
        //    return mLiteralValues[variable];
        //}

        return expr;
    }

    ExprPtr visitNonNullary(const ExprRef<NonNullaryExpr>& expr) override
    {
        ExprVector ops;
        for (size_t i = 0; i < expr->getNumOperands(); ++i) {
            auto newOp = this->visit(expr->getOperand(i));
            ops.push_back(newOp);
        }

        return expr->clone(ops);
    }

    ExprPtr visitNot(const ExprRef<NotExpr>& expr) override
    {
        ExprPtr op = this->visit(expr->getOperand());

        // Not(Not(X)) --> X
        if (op->getKind() == Expr::Not) {
            return llvm::cast<NotExpr>(op.get())->getOperand();
        }

        ExprPtr e1 = nullptr, e2 = nullptr;

        // Not(Eq(E1, E2)) --> NotEq(E1, E2)
        if (match(op, m_Eq(m_Expr(e1), m_Expr(e2)))) {
            return ConstantFolder::NotEq(e1, e2);
        }

        // Not(NotEq(E1, E2)) --> Eq(E1, E2)
        if (match(op, m_NotEq(m_Expr(e1), m_Expr(e2)))) {
            return ConstantFolder::Eq(e1, e2);
        }

        // Not(LESSTHAN(E1, E2)) --> GREATERTHANEQ(E1, E2)
        if (match(op, m_ULt(m_Expr(e1), m_Expr(e2)))) {
            return ConstantFolder::UGtEq(e1, e2);
        }

        if (match(op, m_SLt(m_Expr(e1), m_Expr(e2)))) {
            return ConstantFolder::SGtEq(e1, e2);
        }


        if (op == expr->getOperand()) {
            return expr;
        }

        return ConstantFolder::Not(op);
    }

    ExprPtr visitAnd(const ExprRef<AndExpr>& expr) override
    {
        // Push all 'simple' fact expressions onto a new fact scope
        ExprPtr result = nullptr;
        ExprVector newOps;

        for (const ExprPtr& operand : expr->operands()) {
            ExprPtr newOp = this->visit(operand);
            if (auto lit = llvm::dyn_cast<BoolLiteralExpr>(newOp.get())) {
                if (lit->getValue() == false) {
                    result = BoolLiteralExpr::getFalse();
                    break;
                } else {
                    // We are not adding unnecessary true literals
                }
            } else if (auto andOp = llvm::dyn_cast<AndExpr>(newOp.get())) {
                // Try to flatten the expression
                newOps.insert(newOps.end(), andOp->op_begin(), andOp->op_end());
            } else {
                newOps.push_back(newOp);
            }
        }

        // If we exited early
        if (result != nullptr) {
            return result;
        }

        // If we eliminated all operands
        if (newOps.size() == 0) {
            return BoolLiteralExpr::getTrue();
        } else if (newOps.size() == 1) {
            return newOps[0];
        }

        // Try to collect some facts we can use
        std::vector<LiteralValue> facts;
        for (const ExprPtr& expr : newOps) {
            ExprRef<VarRefExpr> x1;
            ExprRef<LiteralExpr> l1;

            if (match(expr, m_Eq(m_VarRef(x1), m_Literal(l1)))) {
                facts.emplace_back(x1, l1);
            }
        }

        this->combineFacts(facts, newOps, 1);

        if (std::equal(newOps.begin(), newOps.end(), expr->op_begin())) {
            return expr;
        }

        return ConstantFolder::And(newOps);
    }

    ExprPtr visitOr(const ExprRef<OrExpr>& expr) override
    {
        return this->visitNonNullary(expr);
    }

    ExprPtr visitEq(const ExprRef<EqExpr>& expr) override
    {
        ExprPtr lhs = this->visit(expr->getLeft());
        ExprPtr rhs = this->visit(expr->getRight());

        ExprPtr c1 = nullptr;
        ExprPtr e1 = nullptr, e2 = nullptr;

        // Eq(Select(C1, E1, E2), E1) --> C1
        // Eq(Select(C1, E1, E2), E2) --> Not(C1)
        if (unord_match(lhs, rhs, m_Select(m_Expr(c1), m_Expr(e1), m_Expr(e2)), m_Specific(e1))) {
            return c1;
        }

        if (unord_match(lhs, rhs, m_Select(m_Expr(c1), m_Expr(e1), m_Expr(e2)), m_Specific(e2))) {
            return ConstantFolder::Not(c1);
        }

        ExprRef<VarRefExpr> x = nullptr;
        ExprRef<LiteralExpr> l1 = nullptr, l2 = nullptr;

        // Eq(X, L1) if Eq(X, L1) --> True
        // Eq(X, L1) if Eq(X, L2) where L1 != L2 --> False
        // Eq(X, L1) if NotEq(X, L1) --> False
#if 0
        if (unord_match(lhs, rhs, m_VarRef(x), m_Literal(l1))) {
            if (unord_match(mCurrentKnowledge, m_Eq(m_Specific(x), m_Literal(l2)))) {
                mUsedPropagation = true;
                if (l1 == l2) {
                    return BoolLiteralExpr::getTrue();
                }

                return BoolLiteralExpr::getFalse();
            }

            if (unord_match(mCurrentKnowledge, m_NotEq(m_Specific(x), m_Specific(l1)))) {
                return BoolLiteralExpr::getFalse();
            }
        }
#endif
        return expr->clone({ lhs, rhs });
    }

    ExprPtr visitULt(const ExprRef<ULtExpr>& expr) override
    {
        ExprPtr left = this->visit(expr->getLeft());
        ExprPtr right = this->visit(expr->getRight());

        return this->simplifyCompareExpr(expr, left, right);
    }

    ExprPtr visitSelect(const ExprRef<SelectExpr>& expr) override
    {
        ExprPtr cond = this->visit(expr->getCondition());
        ExprPtr then = this->visit(expr->getThen());
        ExprPtr elze = this->visit(expr->getElse());

        // Select(True, E1, E2) --> E1
        // Select(False, E1, E2) --> E2
        if (auto condLit = llvm::dyn_cast<BoolLiteralExpr>(cond.get())) {
            return condLit->isTrue() ? then : elze;
        }

        ExprPtr c1 = nullptr, c2 = nullptr;
        ExprPtr e1 = nullptr, e2 = nullptr;

        // Select(C, E, E) --> E
        if (then == elze) {
            return then;
        }

        // Select(not C, E1, E2) --> Select(C, E2, E1)
        if (match(cond, then, elze, m_Not(m_Expr(c1)), m_Expr(e1), m_Expr(e2))) {
            return ConstantFolder::Select(cond, elze, then);
        }

        // Select(C1, Select(C2, E1, E2), E2) --> Select(C1 and C2, E1, E2)
        if (match(cond, then, elze, m_Expr(c1), m_Select(m_Expr(c2), m_Expr(e1), m_Expr(e2)), m_Specific(e2))) {
            return ConstantFolder::Select(ConstantFolder::And({c1, c2}), e1, e2);
        }

        // Select(C1, E1, Select(C2, E1, E2)) --> Select(C1 or C2, E1, E2)
        if (match(cond, then, elze, m_Expr(c1), m_Expr(e1), m_Select(m_Expr(c2), m_Specific(e1), m_Expr(e2)))) {
            return ConstantFolder::Select(ConstantFolder::Or({c1, c2}), e1, e2);
        }

        return expr->clone({ cond, then, elze });
    }

private:

    bool isSafeToCache() const
    {
        // Caching is only safe if we have not used propagated knowledge
        // TODO
        return true;
        //return !mUsedPropagation;
    }

    ExprPtr simplifyCompareExpr(const ExprRef<NonNullaryExpr>& expr, const ExprPtr& left, const ExprPtr& right);

    void combineFacts(std::vector<LiteralValue> facts, ExprVector& vec, unsigned depth);

private:
    ExprSimplifier::OptLevel mLevel;
    llvm::DenseMap<Expr*, ExprPtr> mVisited;

    unsigned mCurrentDepth = 0;
    unsigned mPropagationDepth = 0;
    unsigned mPropagationLimit = 0;
    ScopedVector<ExprPtr> mCurrentKnowledge;
    bool mUsedPropagation = false;

};

} // end anonymous namespace

ExprPtr ExprSimplifier::simplify(const ExprPtr& expr) const
{
    return ExprSimplifyVisitor(this->mLevel).visit(expr);
}

void ExprSimplifyVisitor::combineFacts(std::vector<LiteralValue> facts, ExprVector &vec, unsigned depth)
{
    // LESSTHAN(X, L1) if Eq(X, L2) where L1 < L2 --> True
    for (auto fact : facts) {
        ExprRef<VarRefExpr>  var = fact.variable;
        ExprRef<LiteralExpr> lit = fact.litVal;

        for (int i = 0; i < vec.size(); ++i) {
            ExprRef<LiteralExpr> l1;

            if (match(vec[i], m_NotEq(m_Specific(var), m_Literal(l1)))) {
                // NotEq(X, L1) if Eq(X, L2) where L1 == L2 --> False
                if (l1 == lit) {
                    vec[0] = BoolLiteralExpr::getFalse();
                    vec.resize(1);
                    return;
                }

                // NotEq(X, L1) if Eq(X, L2) where L1 != L2 --> True
                vec[i] = BoolLiteralExpr::getTrue();
            }
        }
    }
}

ExprPtr ExprSimplifyVisitor::simplifyCompareExpr(const ExprRef<NonNullaryExpr>& expr, const ExprPtr& left, const ExprPtr& right)
{
    assert(expr->getKind() >= Expr::FirstCompare && expr->getKind() <= Expr::LastCompare);

    // Simplify for the case where the two operands of the compare expresison are the same
    if (left == right) {
        switch (expr->getKind()) {
            case Expr::SLt:
            case Expr::SGt:
            case Expr::ULt:
            case Expr::UGt:
            case Expr::NotEq:
                return BoolLiteralExpr::getFalse();
            case Expr::Eq:
            case Expr::SLtEq:
            case Expr::SGtEq:
            case Expr::ULtEq:
            case Expr::UGtEq:
                return BoolLiteralExpr::getTrue();
            default:
                break;
        }
    }

    // CMP(Add(X, C1), C2) -> CMP(X, C2 - C1)
    // CMP(Sub(X, C1), C2) -> CMP(X, C1 + C2)
    ExprRef<> x;
    llvm::APInt c1, c2;

    if (match(left, right, m_Add(m_Bv(&c1), m_Expr(x)), m_Bv(&c2))) {
        return expr->clone({ visit(x), BvLiteralExpr::Get(c2 - c1)});
    }

    if (match(expr, m_ULt(m_Sub(m_Bv(&c1), m_Expr(x)), m_Bv(&c2)))) {
        return expr->clone({ visit(x), BvLiteralExpr::Get(c2 + c1)});
    }

    return expr->clone({ left, right});
}
