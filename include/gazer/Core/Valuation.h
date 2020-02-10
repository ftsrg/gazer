//==- Valuation.h -----------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#ifndef GAZER_CORE_VALUATION_H
#define GAZER_CORE_VALUATION_H

#include "gazer/Core/Expr.h"

#include <llvm/ADT/DenseMap.h>

namespace gazer
{

/// Represents a simple mapping between variables and literal expressions.
class Valuation
{
    using ValuationMapT = llvm::DenseMap<const Variable*, ExprRef<LiteralExpr>>;
public:
    class Builder
    {
    public:
        Valuation build() {
            return Valuation(mMap);
        }

        void put(Variable* variable, const ExprRef<LiteralExpr>& expr) {
            assert((variable->getType() == expr->getType()) && "Types must match.");
            mMap[variable] = expr;
        }

    private:
        ValuationMapT mMap;
    };

    static Builder CreateBuilder() { return Builder(); }

private:
    Valuation(ValuationMapT map)
        : mMap(std::move(map))
    {}
public:
    Valuation() = default;
    Valuation(const Valuation&) = default;
    Valuation& operator=(const Valuation&) = default;
    Valuation(Valuation&&) = default;
    Valuation& operator=(Valuation&&) = default;

public:
    ExprRef<LiteralExpr>& operator[](const Variable& variable);
    ExprRef<LiteralExpr>& operator[](const Variable* variable) {
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
