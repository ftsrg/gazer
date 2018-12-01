#include "gazer/Core/Valuation.h"

#include <llvm/Support/raw_ostream.h>

using namespace gazer;

void Valuation::print(llvm::raw_ostream& os)
{
    for (auto it = mMap.begin(); it != mMap.end(); ++it) {
        Variable* variable = it->first;
        ExprPtr expr = it->second;

        os << variable->getName() << " = ";
        expr->print(os);
        os << "\n";
    }
}

std::shared_ptr<LiteralExpr> Valuation::operator[](const Variable& variable) const
{
    auto result = this->find(&variable);
    if (result == this->end()) {
        // TODO: Maybe there should be check instead of returning a nullptr
        return nullptr;
    }

    return result->second;
}

std::shared_ptr<LiteralExpr> Valuation::eval(const ExprPtr& expr)
{
    if (auto varRef = llvm::dyn_cast<VarRefExpr>(expr.get())) {
        auto lit = this->operator[](varRef->getVariable());
        return lit;
    } else if (expr->getKind() == Expr::Literal) {
        return std::static_pointer_cast<LiteralExpr>(expr);
    } else {
        // TODO: Expand this part
        return nullptr;
    }
}
