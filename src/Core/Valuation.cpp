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