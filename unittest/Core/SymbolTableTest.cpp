#include "gazer/Core/Variable.h"
#include "gazer/Core/SymbolTable.h"

#include <gtest/gtest.h>

using namespace gazer;

TEST(SymbolTable, CanCreateVariables)
{
    SymbolTable st;
    Variable& x = st.create("x", BvType::get(32));
    Variable& y = st.create("y", BoolType::get());

    ASSERT_EQ(x.getName(), "x");
    ASSERT_EQ(y.getName(), "y");

    ASSERT_EQ(x.getType(), *BvType::get(32));
    ASSERT_EQ(y.getType(), *BoolType::get());
}

TEST(SymbolTable, ThrowsExceptionOnDuplicates)
{
    SymbolTable st;
    st.create("x", BvType::get(32));

    EXPECT_THROW(st.create("x", BvType::get(32)), gazer::SymbolAlreadyExistsError);
    EXPECT_THROW(st.create("x", BoolType::get()), gazer::SymbolAlreadyExistsError);
}

TEST(SymbolTable, SubscriptOperatorReturnsVariable)
{
    SymbolTable st;
    st.create("x", BvType::get(32));

    auto& q = st["x"];
    EXPECT_EQ(st["x"].getName(), q.getName());
    EXPECT_EQ(st["x"].getType(), q.getType());
}

TEST(SymbolTable, SubscriptOperatorThrowsExceptionOnInvalidName)
{
    SymbolTable st;
    st.create("x", BvType::get(32));

    EXPECT_NO_THROW(st["x"]);
    EXPECT_THROW(st["y"], SymbolNotFoundError);
}

TEST(SymbolTable, GetReturnsValidOptional)
{
    SymbolTable st;
    auto& x = st.create("x", BvType::get(32));

    auto result = st.get("x");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->get().getName(), x.getName());
}

TEST(SymbolTable, GetReturnsEmpytOptionalOnInvalidName)
{
    SymbolTable st;
    st.create("x", BvType::get(32));

    auto result = st.get("y");
    ASSERT_FALSE(result);
}
