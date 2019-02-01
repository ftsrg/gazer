#include "gazer/Core/Type.h"

#include "gazer/Core/GazerContext.h"

#include <gtest/gtest.h>

using namespace gazer;


TEST(TypeTest, PrintType)
{
    GazerContext context;

    BoolType& boolTy = BoolType::Get(context);
    EXPECT_EQ(boolTy.getName(), "Bool");
    
    BvType& intTy = BvType::Get(context, 32);
    EXPECT_EQ(intTy.getName(), "Bv32");

    BvType& bvTy2 = BvType::Get(context, 57);
    EXPECT_EQ(bvTy2.getName(), "Bv57");

    //ArrayType& arrTy = ArrayType::Get(intTy, intTy);
    //EXPECT_EQ(arrTy.getName(), "[Bv32 -> Bv32]");

    //ArrayType& arr2Ty = ArrayType::Get(intTy, boolTy);
    //EXPECT_EQ(arr2Ty.getName(), "[Bv32 -> Bool]");
}

TEST(TypeTest, TypeEquals)
{
    GazerContext context;

    BoolType& boolTy = BoolType::Get(context);
    BvType& intTy = BvType::Get(context, 32);
    
    EXPECT_TRUE(boolTy == boolTy);
    EXPECT_TRUE(intTy == intTy);

    EXPECT_FALSE(boolTy == intTy);
    EXPECT_FALSE(intTy == boolTy);

/*
    ArrayType& arrTy = ArrayType::Get(intTy, intTy);
    ArrayType& arr2Ty = ArrayType::Get(intTy, boolTy);

    EXPECT_TRUE(arrTy == arrTy);
    EXPECT_FALSE(arr2Ty == arrTy);
    EXPECT_FALSE(arr2Ty == intTy);

    ArrayType& arr3Ty = ArrayType::Get(intTy, intTy);
    
    EXPECT_TRUE(arr3Ty == arrTy);
    FunctionType& funcType = FunctionType::Get(boolTy, {intTy, intTy});
    EXPECT_TRUE(funcType == funcType);

    FunctionType& funcType2 = FunctionType::Get(intTy, {intTy, intTy});
    EXPECT_FALSE(funcType == funcType2);

    FunctionType& funcType3 = FunctionType::Get(boolTy, {boolTy});
    EXPECT_FALSE(funcType == funcType3);

    FunctionType& funcType4 = FunctionType::Get(boolTy, {intTy, intTy});
    EXPECT_TRUE(funcType == funcType4);
*/
}
