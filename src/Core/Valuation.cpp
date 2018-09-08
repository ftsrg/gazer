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
