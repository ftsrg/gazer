#include "gazer/Core/Type.h"

#include "gazer/Core/GazerContext.h"

#include <gtest/gtest.h>

using namespace gazer;

TEST(TypeTest, PrintType)
{
    GazerContext context;

    BoolType& boolTy = BoolType::Get(context);
    EXPECT_EQ(boolTy.getName(), "Bool");
    
    BvType& bvTy32 = BvType::Get(context, 32);
    EXPECT_EQ(bvTy32.getName(), "Bv32");

    BvType& bvTy57 = BvType::Get(context, 57);
    EXPECT_EQ(bvTy57.getName(), "Bv57");

    FloatType& halfTy = FloatType::Get(context, FloatType::Half);
    EXPECT_EQ(halfTy.getName(), "Float16");

    FloatType& floatTy = FloatType::Get(context, FloatType::Single);
    EXPECT_EQ(floatTy.getName(), "Float32");

    FloatType& doubleTy = FloatType::Get(context, FloatType::Double);
    EXPECT_EQ(doubleTy.getName(), "Float64");

    IntType& intTy = IntType::Get(context);
    EXPECT_EQ(intTy.getName(), "Int");

    RealType& realTy = RealType::Get(context);
    EXPECT_EQ(realTy.getName(), "Real");
}

TEST(TypeTest, TypeEquals)
{
    GazerContext context;

    BoolType& boolTy = BoolType::Get(context);
    BvType& bvTy32 = BvType::Get(context, 32);
    BvType& bvTy57 = BvType::Get(context, 57);
    FloatType& floatTy = FloatType::Get(context, FloatType::Single);
    FloatType& doubleTy = FloatType::Get(context, FloatType::Double);
    
    EXPECT_TRUE(boolTy == boolTy);
    EXPECT_TRUE(bvTy32 == bvTy32);
    EXPECT_TRUE(floatTy == floatTy);

    EXPECT_FALSE(boolTy == bvTy32);
    EXPECT_FALSE(bvTy32 == boolTy);
    EXPECT_FALSE(bvTy32 == bvTy57);
    EXPECT_FALSE(floatTy == doubleTy);
}

TEST(TypeTest, FloatPrecision)
{
    GazerContext context;

    FloatType& halfTy = FloatType::Get(context, FloatType::Half);
    FloatType& floatTy = FloatType::Get(context, FloatType::Single);
    FloatType& doubleTy = FloatType::Get(context, FloatType::Double);
    FloatType& quadTy = FloatType::Get(context, FloatType::Quad);

    EXPECT_EQ(halfTy.getWidth(), 16);
    EXPECT_EQ(floatTy.getWidth(), 32);
    EXPECT_EQ(doubleTy.getWidth(), 64);
    EXPECT_EQ(quadTy.getWidth(), 128);
}

TEST(TypeTest, BvPrecision)
{
    GazerContext context;

    BvType& bvTy1  = BvType::Get(context, 1);
    BvType& bvTy32 = BvType::Get(context, 32);
    BvType& bvTy57 = BvType::Get(context, 57);

    EXPECT_EQ(bvTy1.getWidth(), 1);
    EXPECT_EQ(bvTy32.getWidth(), 32);
    EXPECT_EQ(bvTy57.getWidth(), 57);
}