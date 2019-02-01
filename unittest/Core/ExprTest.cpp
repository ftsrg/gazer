#include "gazer/Core/Expr.h"
#include "gazer/Core/GazerContext.h"
#include "gazer/Core/Variable.h"
#include "gazer/Core/ExprTypes.h"

#include <gtest/gtest.h>

using namespace gazer;

TEST(Expr, CanCreateExpressions)
{
    GazerContext context;

    auto x = context.createVariable("X", BvType::Get(context, 32))->getRefExpr();
    auto y = context.createVariable("Y", BvType::Get(context, 32))->getRefExpr();

    auto add = EqExpr::Create(x, y);
    auto add2 = EqExpr::Create(x, y);

    ASSERT_EQ(add, add2);
}
