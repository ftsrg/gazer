#include "gazer/Core/Utils/ExprBuilder.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/ADT/APInt.h>

using namespace gazer;

namespace
{

class DefaultExprBuilder : public ExprBuilder
{
public:
    DefaultExprBuilder()
        : ExprBuilder()
    {}

    ExprPtr Not(const ExprPtr& op) override {
        return NotExpr::Create(op);
    }

    ExprPtr Add(const ExprPtr& left, const ExprPtr& right) override {
        return AddExpr::Create(left, right);
    }
    ExprPtr Sub(const ExprPtr& left, const ExprPtr& right) override {
        return SubExpr::Create(left, right);
    }
    ExprPtr Mul(const ExprPtr& left, const ExprPtr& right) override {
        return MulExpr::Create(left, right);
    }
    ExprPtr Div(const ExprPtr& left, const ExprPtr& right) override {
        return DivExpr::Create(left, right);
    }

    ExprPtr And(const ExprPtr& left, const ExprPtr& right) override {
        return AndExpr::Create(left, right);
    }
    ExprPtr Or(const ExprPtr& left, const ExprPtr& right) override {
        return OrExpr::Create(left, right);
    }
    ExprPtr Xor(const ExprPtr& left, const ExprPtr& right) override {
        return XorExpr::Create(left, right);
    }

    ExprPtr Eq(const ExprPtr& left, const ExprPtr& right) override {
        return EqExpr::Create(left, right);
    }
    ExprPtr NotEq(const ExprPtr& left, const ExprPtr& right) override {
        return NotEqExpr::Create(left, right);
    }
    ExprPtr Lt(const ExprPtr& left, const ExprPtr& right) override {
        return LtExpr::Create(left, right);
    }
    ExprPtr LtEq(const ExprPtr& left, const ExprPtr& right) override {
        return LtEqExpr::Create(left, right);
    }
    ExprPtr Gt(const ExprPtr& left, const ExprPtr& right) override {
        return GtExpr::Create(left, right);
    }
    ExprPtr GtEq(const ExprPtr& left, const ExprPtr& right) override {
        return GtEqExpr::Create(left, right);
    }
    ExprPtr Select(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze) override {
        return SelectExpr::Create(condition, then, elze);
    }

};

} // end anonymous namespace

std::unique_ptr<ExprBuilder> gazer::CreateExprBuilder() {
    return std::unique_ptr<ExprBuilder>(new DefaultExprBuilder());
}
