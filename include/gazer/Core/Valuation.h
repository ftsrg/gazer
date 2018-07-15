#ifndef _GAZER_CORE_VALUATION_H
#define _GAZER_CORE_VALUATION_H

#include "gazer/Core/Variable.h"
#include "gazer/Core/SymbolTable.h"

#include <llvm/ADT/DenseMap.h>

namespace gazer
{

class Valuation
{
    using ValuationMapT = llvm::DenseMap<Variable*, std::shared_ptr<LiteralExpr>>;
public:
    class Builder;
public:
    std::shared_ptr<LiteralExpr> eval(const ExprPtr& expr);

private:
    ValuationMapT mMap;
};

/**
 * Builder class for valuations.
 */
class Valuation::Builder
{
public:
    Valuation build();

    void put(Variable* variable, const std::shared_ptr<LiteralExpr>& expr) {
        assert((variable->getType() == expr->getType()) && "Types must match.");
        mMap[variable] = expr;
    }

private:
    ValuationMapT mMap;
};

}

#endif
