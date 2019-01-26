#ifndef _GAZER_CORE_EXPR_EXPRSIMPLIFY_H
#define _GAZER_CORE_EXPR_EXPRSIMPLIFY_H

#include "gazer/Core/Expr.h"

namespace gazer
{

class ExprSimplifier
{
public:
    enum OptLevel { Simple, Aggressive, Expensive };

    ExprSimplifier(OptLevel level)
        : mLevel(level)
    {}

    ExprPtr simplify(const ExprPtr& expr) const;

private:
    OptLevel mLevel;   
};

}

#endif
