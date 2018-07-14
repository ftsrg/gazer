#ifndef _GAZER_CORE_UTILS_EXPRSIMPLIFY_H
#define _GAZER_CORE_UTILS_EXPRSIMPLIFY_H

#include "gazer/Core/ExprVisitor.h"

namespace gazer
{

class ExprSimplify final : public ExprVisitor<ExprPtr>
{

};

}

#endif
