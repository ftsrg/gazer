#ifndef _GAZER_CORE_UTILS_VALUATIONSTACK_H
#define _GAZER_CORE_UTILS_VALUATIONSTACK_H

#include "gazer/Core/Valuation.h"

#include <stack>

namespace gazer
{

class ValuationStack final
{
public:
    ValuationStack();

private:
    void push(const Valuation& val);
    void pop();

    using iterator = Valuation::iterator;

    iterator find(const Variable* variable);

private:
    std::stack<Valuation> mStack;
};

}

#endif
