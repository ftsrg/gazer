#include "gazer/Core/Type.h"

#include <gtest/gtest.h>

using namespace gazer;

TEST(Type, PrintType)
{
    BoolType* boolTy = BoolType::get();
    EXPECT_EQ(boolTy->getName(), "Bool");
    
    BvType* intTy = BvType::get(32);
    EXPECT_EQ(intTy->getName(), "Int");

    ArrayType* arrTy = ArrayType::get(intTy, intTy);
    EXPECT_EQ(arrTy->getName(), "[Int -> Int]");

    ArrayType* arr2Ty = ArrayType::get(intTy, boolTy);
    EXPECT_EQ(arr2Ty->getName(), "[Int -> Bool]");

    FunctionType* funcType = FunctionType::get(boolTy, {intTy, intTy});
    EXPECT_EQ(funcType->getName(), "(Int, Int) -> Bool");
}

TEST(Type, TypeEquals)
{
    BoolType* boolTy = BoolType::get();
    BvType* intTy = BvType::get(32);
    
    EXPECT_TRUE(boolTy->equals(boolTy));
    EXPECT_TRUE(intTy->equals(intTy));

    EXPECT_FALSE(boolTy->equals(intTy));
    EXPECT_FALSE(intTy->equals(boolTy));

    ArrayType* arrTy = ArrayType::get(intTy, intTy);
    ArrayType* arr2Ty = ArrayType::get(intTy, boolTy);

    EXPECT_TRUE(arrTy->equals(arrTy));
    EXPECT_FALSE(arr2Ty->equals(arrTy));
    EXPECT_FALSE(arr2Ty->equals(intTy));

    ArrayType* arr3Ty = ArrayType::get(intTy, intTy);
    
    EXPECT_TRUE(arr3Ty->equals(arrTy));

    FunctionType* funcType = FunctionType::get(boolTy, {intTy, intTy});
    EXPECT_TRUE(funcType->equals(funcType));

    FunctionType* funcType2 = FunctionType::get(intTy, {intTy, intTy});
    EXPECT_FALSE(funcType->equals(funcType2));

    FunctionType* funcType3 = FunctionType::get(boolTy, {boolTy});
    EXPECT_FALSE(funcType->equals(funcType3));

    FunctionType* funcType4 = FunctionType::get(boolTy, {intTy, intTy});
    EXPECT_TRUE(funcType->equals(funcType4));
}
