#ifndef _GAZER_CORE_VALUATION_H
#define _GAZER_CORE_VALUATION_H

#include "gazer/Core/Variable.h"
#include "gazer/Core/SymbolTable.h"

#include <llvm/ADT/DenseMap.h>

namespace gazer
{

/**
 * Represents a simple mapping between variables and literal expressions.
 */
class Valuation
{
    using ValuationMapT = llvm::DenseMap<Variable*, std::shared_ptr<LiteralExpr>>;
public:
    class Builder
    {
    public:
        Valuation build() {
            return Valuation(mMap);
        }

        void put(Variable* variable, const std::shared_ptr<LiteralExpr>& expr) {
            assert((variable->getType() == expr->getType()) && "Types must match.");
            mMap[variable] = expr;
        }

    private:
        ValuationMapT mMap;
    };

    static Builder CreateBuilder() { return Builder(); }

private:
    Valuation(ValuationMapT map)
        : mMap(map)
    {}
public:
    Valuation(const Valuation&) = default;
    Valuation& operator=(const Valuation&) = default;
public:
    std::shared_ptr<LiteralExpr> eval(const ExprPtr& expr);
    std::shared_ptr<LiteralExpr> operator[](const Variable& variable);
    std::shared_ptr<LiteralExpr> operator[](const Variable* variable) {
        return operator[](*variable);
    }
    
    using iterator = ValuationMapT::iterator;
    using const_iterator = ValuationMapT::const_iterator;

    iterator find(const Variable* variable) { return mMap.find(variable); }
    const_iterator find(const Variable* variable) const { return mMap.find(variable); }

    iterator begin() { return mMap.begin(); }
    iterator end() { return mMap.end(); }
    const_iterator begin() const { return mMap.begin(); }
    const_iterator end() const { return mMap.end(); }

    void print(llvm::raw_ostream& os);

private:
    ValuationMapT mMap;
};

}

#endif
