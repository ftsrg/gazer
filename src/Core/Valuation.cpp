#include "gazer/Core/Valuation.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/Support/raw_ostream.h>

using namespace gazer;

void Valuation::print(llvm::raw_ostream& os)
{
    for (auto it = mMap.begin(); it != mMap.end(); ++it) {
        auto [variable, expr] = *it;

        os << variable->getName() << " = ";
        expr->print(os);
        os << "\n";
    }
}

ExprRef<LiteralExpr>& Valuation::operator[](const Variable& variable)
{
    return mMap[&variable];
}

ExprRef<AtomicExpr> Valuation::eval(const ExprPtr& expr)
{
    if (auto varRef = llvm::dyn_cast<VarRefExpr>(expr.get())) {
        auto lit = this->operator[](varRef->getVariable());
        return lit != nullptr ? boost::static_pointer_cast<AtomicExpr>(lit) : UndefExpr::Get(expr->getType());
    }
    
    if (expr->getKind() == Expr::Literal) {
        return llvm::cast<LiteralExpr>(expr);
    }

    return UndefExpr::Get(expr->getType());
}
