#include "gazer/Core/Utils/ValuationStack.h"

using namespace gazer;

auto ValuationStack::find(const Variable* variable) -> iterator
{
    while (!mStack.size() != 1) {
        auto result = mStack.top().find(variable);
        if (result != mStack.top().end()) {

        }
    }
}