#include "gazer/Core/Variable.h"

#include <gtest/gtest.h>

using namespace gazer;

TEST(Variable, CanCreateVariables)
{
    GazerContext context;
    Variable* x = context.createVariable("x", BvType::Get(context, 32));
    Variable* y = context.createVariable("y", BoolType::Get(context));

    ASSERT_EQ(x->getName(), "x");
    ASSERT_EQ(y->getName(), "y");

    ASSERT_EQ(x->getType(), BvType::Get(context, 32));
    ASSERT_EQ(y->getType(), BoolType::Get(context));
}
