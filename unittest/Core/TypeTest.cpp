#include "gazer/Core/Type.h"

#include <gtest/gtest.h>

using namespace gazer;

TEST(Type, PrintType)
{
    BoolType& boolTy = BoolType::get();
    EXPECT_EQ(boolTy.getName(), "Bool");
    
    BvType& intTy = BvType::get(32);
    EXPECT_EQ(intTy.getName(), "Bv32");

    ArrayType& arrTy = ArrayType::get(intTy, intTy);
    EXPECT_EQ(arrTy.getName(), "[Bv32 -> Bv32]");

    ArrayType& arr2Ty = ArrayType::get(intTy, boolTy);
    EXPECT_EQ(arr2Ty.getName(), "[Bv32 -> Bool]");
/*
    FunctionType& funcType = FunctionType::get(boolTy, {intTy, intTy});
    EXPECT_EQ(funcType->getName(), "(Int, Int) -> Bool");
*/
}

TEST(Type, TypeEquals)
{
    BoolType& boolTy = BoolType::get();
    BvType& intTy = BvType::get(32);
    
    EXPECT_TRUE(boolTy == boolTy);
    EXPECT_TRUE(intTy == intTy);

    EXPECT_FALSE(boolTy == intTy);
    EXPECT_FALSE(intTy == boolTy);

    ArrayType& arrTy = ArrayType::get(intTy, intTy);
    ArrayType& arr2Ty = ArrayType::get(intTy, boolTy);

    EXPECT_TRUE(arrTy == arrTy);
    EXPECT_FALSE(arr2Ty == arrTy);
    EXPECT_FALSE(arr2Ty == intTy);

    ArrayType& arr3Ty = ArrayType::get(intTy, intTy);
    
    EXPECT_TRUE(arr3Ty == arrTy);
/*
    FunctionType& funcType = FunctionType::get(boolTy, {intTy, intTy});
    EXPECT_TRUE(funcType == funcType);

    FunctionType& funcType2 = FunctionType::get(intTy, {intTy, intTy});
    EXPECT_FALSE(funcType == funcType2);

    FunctionType& funcType3 = FunctionType::get(boolTy, {boolTy});
    EXPECT_FALSE(funcType == funcType3);

    FunctionType& funcType4 = FunctionType::get(boolTy, {intTy, intTy});
    EXPECT_TRUE(funcType == funcType4);
*/
}
