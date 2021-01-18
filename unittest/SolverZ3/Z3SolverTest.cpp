//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Core/Solver/Model.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/Support/raw_ostream.h>

#include <gtest/gtest.h>

using namespace gazer;

namespace {

class SolverZ3Test : public ::testing::Test
{
public:
    void SetUp() override
    {
        tmpCount = 0;
        solver = factory.createSolver(ctx);
    }

    ::testing::AssertionResult checkBvOp(const ExprPtr& op, const ExprPtr& expected);

protected:
    GazerContext ctx;
    Z3SolverFactory factory;
    std::unique_ptr<Solver> solver;
    unsigned tmpCount = 0;
};

TEST_F(SolverZ3Test, SmokeTest1)
{
    auto a = ctx.createVariable("A", BoolType::Get(ctx));
    auto b = ctx.createVariable("B", BoolType::Get(ctx));

    // (A & B)
    solver->add(AndExpr::Create(a->getRefExpr(), b->getRefExpr()));

    auto result = solver->run();

    ASSERT_EQ(result, Solver::SAT);
    auto model = solver->getModel();

    ASSERT_EQ(model->evaluate(a->getRefExpr()), BoolLiteralExpr::True(ctx));
    ASSERT_EQ(model->evaluate(b->getRefExpr()), BoolLiteralExpr::True(ctx));
}

TEST_F(SolverZ3Test, BooleansUnsat)
{
    auto x = ctx.createVariable("x", BoolType::Get(ctx))->getRefExpr();
    auto y = ctx.createVariable("y", BoolType::Get(ctx))->getRefExpr();

    auto deMorgan = EqExpr::Create(
        AndExpr::Create(x, y),
        NotExpr::Create(OrExpr::Create(NotExpr::Create(x), NotExpr::Create(y))));

    solver->add(NotExpr::Create(deMorgan));

    auto result = solver->run();

    ASSERT_EQ(result, Solver::UNSAT);
}

TEST_F(SolverZ3Test, Undef)
{
    auto x = ctx.createVariable("x", BoolType::Get(ctx))->getRefExpr();
    auto y = ctx.createVariable("y", BoolType::Get(ctx))->getRefExpr();

    auto f1 = AndExpr::Create(x, UndefExpr::Get(BoolType::Get(ctx)));

    solver->add(f1);

    auto status = solver->run();
    ASSERT_EQ(status, Solver::SAT);

    solver->add(NotExpr::Create(f1));
    status = solver->run();

    // TODO(sallaigy): Fix this test if we implement undef lifting for CFA. See comment in
    //  Z3SolverImpl::visitUndef for more details.
    ASSERT_EQ(status, Solver::UNSAT);
}

TEST_F(SolverZ3Test, Integers)
{
    auto x = ctx.createVariable("x", IntType::Get(ctx))->getRefExpr();
    auto y = ctx.createVariable("y", IntType::Get(ctx))->getRefExpr();

    // 2x + y = 11
    // 3x - y = 9
    solver->add(EqExpr::Create(
        AddExpr::Create(MulExpr::Create(IntLiteralExpr::Get(ctx, 2), x), y),
        IntLiteralExpr::Get(IntType::Get(ctx), 11)));
    solver->add(EqExpr::Create(
        SubExpr::Create(MulExpr::Create(IntLiteralExpr::Get(ctx, 3), x), y),
        IntLiteralExpr::Get(ctx, 9)));

    auto status = solver->run();
    ASSERT_EQ(status, Solver::SAT);

    auto model = solver->getModel();
    ASSERT_EQ(model->evaluate(x), IntLiteralExpr::Get(ctx, 4));
    ASSERT_EQ(model->evaluate(y), IntLiteralExpr::Get(ctx, 3));
}

TEST_F(SolverZ3Test, IntegerOperations)
{
    EXPECT_TRUE(checkBvOp(
        DivExpr::Create(IntLiteralExpr::Get(ctx, 7), IntLiteralExpr::Get(ctx, 3)), IntLiteralExpr::Get(ctx, 2)));
    EXPECT_TRUE(checkBvOp(
        RemExpr::Create(IntLiteralExpr::Get(ctx, 7), IntLiteralExpr::Get(ctx, 3)), IntLiteralExpr::Get(ctx, 1)));
    EXPECT_TRUE(checkBvOp(
        ModExpr::Create(IntLiteralExpr::Get(ctx, 7), IntLiteralExpr::Get(ctx, 3)), IntLiteralExpr::Get(ctx, 1)));
}

TEST_F(SolverZ3Test, IntegerCompare)
{
    auto left = IntLiteralExpr::Get(ctx, -5);
    auto right = IntLiteralExpr::Get(ctx, 2);

    EXPECT_TRUE(checkBvOp(
        LtExpr::Create(left, right), BoolLiteralExpr::True(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        LtEqExpr::Create(left, right), BoolLiteralExpr::True(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        LtEqExpr::Create(left, left), BoolLiteralExpr::True(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        GtExpr::Create(left, right), BoolLiteralExpr::False(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        GtExpr::Create(right, left), BoolLiteralExpr::True(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        GtEqExpr::Create(left, right), BoolLiteralExpr::False(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        GtEqExpr::Create(left, left), BoolLiteralExpr::True(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        EqExpr::Create(left, right), BoolLiteralExpr::False(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        EqExpr::Create(left, left), BoolLiteralExpr::True(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        NotEqExpr::Create(left, right), BoolLiteralExpr::True(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        NotEqExpr::Create(left, left), BoolLiteralExpr::False(ctx)
    ));
}

::testing::AssertionResult SolverZ3Test::checkBvOp(const ExprPtr& op, const ExprPtr& expected)
{
    solver->reset();

    auto x =
        ctx.createVariable("x" + std::to_string(tmpCount++), expected->getType())->getRefExpr();
    solver->add(EqExpr::Create(x, op));

    auto status = solver->run();
    EXPECT_EQ(status, Solver::SAT);

    auto model = solver->getModel();
    auto value = model->evaluate(x);

    if (value != expected) {
        std::string buffer;
        llvm::raw_string_ostream rso(buffer);

        rso << "Expected " << *expected << " to be equal to actual value " << *value << ", but it was not.\n";
        rso.flush();

        return ::testing::AssertionFailure() << rso.str();
    }

    return ::testing::AssertionSuccess();
}

TEST_F(SolverZ3Test, Bitvectors)
{
    auto& bv32Ty = BvType::Get(ctx, 32);

    auto seven = BvLiteralExpr::Get(bv32Ty, 7);
    auto three = BvLiteralExpr::Get(bv32Ty, 3);

    EXPECT_TRUE(checkBvOp(AddExpr::Create(seven, three), BvLiteralExpr::Get(bv32Ty, 10)));
    EXPECT_TRUE(checkBvOp(SubExpr::Create(seven, three), BvLiteralExpr::Get(bv32Ty, 4)));
    EXPECT_TRUE(checkBvOp(MulExpr::Create(seven, three), BvLiteralExpr::Get(bv32Ty, 21)));
    EXPECT_TRUE(checkBvOp(BvSRemExpr::Create(seven, three), BvLiteralExpr::Get(bv32Ty, 1)));
    EXPECT_TRUE(checkBvOp(BvURemExpr::Create(seven, three), BvLiteralExpr::Get(bv32Ty, 1)));
    EXPECT_TRUE(checkBvOp(ShlExpr::Create(seven, three), BvLiteralExpr::Get(bv32Ty, 56)));
    EXPECT_TRUE(checkBvOp(AShrExpr::Create(seven, three), BvLiteralExpr::Get(bv32Ty, 0)));
    EXPECT_TRUE(checkBvOp(LShrExpr::Create(seven, three), BvLiteralExpr::Get(bv32Ty, 0)));
    EXPECT_TRUE(checkBvOp(BvSDivExpr::Create(seven, three), BvLiteralExpr::Get(bv32Ty, 2)));
    EXPECT_TRUE(checkBvOp(BvUDivExpr::Create(seven, three), BvLiteralExpr::Get(bv32Ty, 2)));
    EXPECT_TRUE(checkBvOp(BvAndExpr::Create(seven, three), three));
    EXPECT_TRUE(checkBvOp(BvOrExpr::Create(seven, three), seven));
    EXPECT_TRUE(checkBvOp(BvXorExpr::Create(seven, three),BvLiteralExpr::Get(bv32Ty, 4)));

    auto& bv16Ty = BvType::Get(ctx, 16);

    // 458752 + 3
    checkBvOp(
        BvConcatExpr::Create(BvLiteralExpr::Get(bv16Ty, 7), BvLiteralExpr::Get(bv16Ty, 3)),
        BvLiteralExpr::Get(bv32Ty, 458755));
}

TEST_F(SolverZ3Test, BitVectorCompare)
{
    auto& bv32Ty = BvType::Get(ctx, 32);

    auto left = BvLiteralExpr::Get(bv32Ty, llvm::APInt(32, "-5", 10));
    auto right = BvLiteralExpr::Get(bv32Ty, llvm::APInt(32, "2", 10));

    // -5 slt 2 = True
    // -5 slt -5 = False
    // -5 ult 2 --> 4294967291 < 2 = False
    EXPECT_TRUE(checkBvOp(BvSLtExpr::Create(left, right), BoolLiteralExpr::True(ctx)));
    EXPECT_TRUE(checkBvOp(BvSLtExpr::Create(right, right), BoolLiteralExpr::False(ctx)));
    EXPECT_TRUE(checkBvOp(BvULtExpr::Create(left, right), BoolLiteralExpr::False(ctx)));
    EXPECT_TRUE(checkBvOp(BvSGtExpr::Create(left, right), BoolLiteralExpr::False(ctx)));
    EXPECT_TRUE(checkBvOp(BvSGtExpr::Create(right, right), BoolLiteralExpr::False(ctx)));
    EXPECT_TRUE(checkBvOp(BvUGtExpr::Create(left, right), BoolLiteralExpr::True(ctx)));

    // -5 sle 2 = True
    // -5 sle -5 = True
    // -5 sle -6 = False
    // -5 ule 2 = False
    EXPECT_TRUE(checkBvOp(BvSLtEqExpr::Create(left, right), BoolLiteralExpr::True(ctx)));
    EXPECT_TRUE(checkBvOp(BvSLtEqExpr::Create(right, right), BoolLiteralExpr::True(ctx)));
    EXPECT_TRUE(checkBvOp(BvULtEqExpr::Create(left, BvLiteralExpr::Get(bv32Ty, llvm::APInt(32, "-6", 10))), BoolLiteralExpr::False(ctx)));
    EXPECT_TRUE(checkBvOp(BvULtEqExpr::Create(left, right), BoolLiteralExpr::False(ctx)));

    EXPECT_TRUE(checkBvOp(BvSGtEqExpr::Create(left, right), BoolLiteralExpr::False(ctx)));
    EXPECT_TRUE(checkBvOp(BvSGtEqExpr::Create(right, right), BoolLiteralExpr::True(ctx)));
    EXPECT_TRUE(checkBvOp(BvUGtEqExpr::Create(left, BvLiteralExpr::Get(bv32Ty, llvm::APInt(32, "-6", 10))), BoolLiteralExpr::True(ctx)));
    EXPECT_TRUE(checkBvOp(BvUGtEqExpr::Create(left, right), BoolLiteralExpr::True(ctx)));

    EXPECT_TRUE(checkBvOp(EqExpr::Create(left, left), BoolLiteralExpr::True(ctx)));
    EXPECT_TRUE(checkBvOp(EqExpr::Create(left, right), BoolLiteralExpr::False(ctx)));
}

TEST_F(SolverZ3Test, BvCasts)
{
    auto x = ctx.createVariable("x", BvType::Get(ctx, 16))->getRefExpr();
    auto y1 = ctx.createVariable("y1", BvType::Get(ctx, 32))->getRefExpr();
    auto y2 = ctx.createVariable("y2", BvType::Get(ctx, 32))->getRefExpr();

    solver->add(EqExpr::Create(x, BvLiteralExpr::Get(BvType::Get(ctx, 16), 65535)));
    solver->add(EqExpr::Create(y1, ZExtExpr::Create(x, BvType::Get(ctx, 32))));
    solver->add(EqExpr::Create(y2, SExtExpr::Create(x, BvType::Get(ctx, 32))));

    auto status = solver->run();
    ASSERT_EQ(status, Solver::SAT);

    auto model = solver->getModel();
    EXPECT_EQ(model->evaluate(y1), BvLiteralExpr::Get(BvType::Get(ctx, 32), 65535));
    EXPECT_EQ(model->evaluate(y2), BvLiteralExpr::Get(BvType::Get(ctx, 32), llvm::APInt(32, "-1", 10)));
}

TEST_F(SolverZ3Test, BvExtract)
{
    auto x = ctx.createVariable("x", BvType::Get(ctx, 16))->getRefExpr();
    auto y1 = ctx.createVariable("y1", BvType::Get(ctx, 8))->getRefExpr();
    auto y2 = ctx.createVariable("y2", BvType::Get(ctx, 8))->getRefExpr();

    // 1111 1111 0000 0000
    solver->add(EqExpr::Create(x, BvLiteralExpr::Get(BvType::Get(ctx, 16), 65280)));
    solver->add(EqExpr::Create(y1, ExtractExpr::Create(x, 0, 8)));
    solver->add(EqExpr::Create(y2, ExtractExpr::Create(x, 8, 8)));


    auto status = solver->run();
    ASSERT_EQ(status, Solver::SAT);

    auto model = solver->getModel();
    EXPECT_EQ(model->evaluate(y1), BvLiteralExpr::Get(BvType::Get(ctx, 8), 0));
    EXPECT_EQ(model->evaluate(y2), BvLiteralExpr::Get(BvType::Get(ctx, 8), 255));
}

TEST_F(SolverZ3Test, FloatLiterals)
{
    auto& fp16Ty = FloatType::Get(ctx, FloatType::Half);
    auto& fp32Ty = FloatType::Get(ctx, FloatType::Single);
    auto& fp64Ty = FloatType::Get(ctx, FloatType::Double);
    auto& fp128Ty = FloatType::Get(ctx, FloatType::Quad);

    auto largeFp16 = FloatLiteralExpr::Get(fp16Ty, llvm::APFloat::getLargest(fp16Ty.getLLVMSemantics()));
    auto smallFp16 = FloatLiteralExpr::Get(fp16Ty, llvm::APFloat::getSmallestNormalized(fp16Ty.getLLVMSemantics()));

    auto x16 = ctx.createVariable("x16", fp16Ty)->getRefExpr();
    auto y16 = ctx.createVariable("y16", fp16Ty)->getRefExpr();

    solver->add(FEqExpr::Create(x16, largeFp16));
    solver->add(FEqExpr::Create(y16, smallFp16));

    auto status = solver->run();
    ASSERT_EQ(status, Solver::SAT);

    auto model = solver->getModel();
    EXPECT_EQ(model->evaluate(x16), largeFp16);
    EXPECT_EQ(model->evaluate(y16), smallFp16);
}

TEST_F(SolverZ3Test, FpaWithRoundingMode)
{
    // fcast.fp64(tmp) == 0
    auto tmp = ctx.createVariable("tmp", FloatType::Get(ctx, FloatType::Single));
    auto fcast = FCastExpr::Create(
        tmp->getRefExpr(), FloatType::Get(ctx, FloatType::Double),
        llvm::APFloatBase::rmNearestTiesToEven);
    auto eq = FEqExpr::Create(
        fcast, FloatLiteralExpr::Get(FloatType::Get(ctx, FloatType::Double), llvm::APFloat{0.0}));

    solver->add(eq);

    auto result = solver->run();
    ASSERT_EQ(result, Solver::SAT);
    auto model = solver->getModel();

    ASSERT_EQ(
        model->evaluate(tmp->getRefExpr()),
        FloatLiteralExpr::Get(FloatType::Get(ctx, FloatType::Single), llvm::APFloat(0.0f)));
}

TEST_F(SolverZ3Test, FloatOperations)
{
    auto& fp32Ty = FloatType::Get(ctx, FloatType::Single);

    EXPECT_TRUE(checkBvOp(FAddExpr::Create(
        FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(7.0f)),
        FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(3.5f)),
        llvm::APFloat::rmNearestTiesToEven
    ), FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(10.5f))));
    EXPECT_TRUE(checkBvOp(FSubExpr::Create(
        FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(7.0f)),
        FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(3.5f)),
        llvm::APFloat::rmNearestTiesToEven
    ), FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(3.5f))));
    EXPECT_TRUE(checkBvOp(FMulExpr::Create(
        FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(7.0f)),
        FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(3.5f)),
        llvm::APFloat::rmNearestTiesToEven
    ), FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(24.5f))));
    EXPECT_TRUE(checkBvOp(FDivExpr::Create(
        FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(7.0f)),
        FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(3.5f)),
        llvm::APFloat::rmNearestTiesToEven
    ), FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(2.0f))));
}

TEST_F(SolverZ3Test, FloatCompare)
{
    auto& fp32Ty = FloatType::Get(ctx, FloatType::Single);

    auto left = FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(-0.5f));
    auto right = FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(6.5f));

    EXPECT_TRUE(checkBvOp(
        FEqExpr::Create(left, right), BoolLiteralExpr::False(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        FEqExpr::Create(left, left), BoolLiteralExpr::True(ctx)
    ));

    EXPECT_TRUE(checkBvOp(
        FGtExpr::Create(left, right), BoolLiteralExpr::False(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        FGtExpr::Create(right, left), BoolLiteralExpr::True(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        FGtEqExpr::Create(right, left), BoolLiteralExpr::True(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        FGtEqExpr::Create(left, left), BoolLiteralExpr::True(ctx)
    ));

    EXPECT_TRUE(checkBvOp(
        FLtExpr::Create(left, right), BoolLiteralExpr::True(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        FLtExpr::Create(right, left), BoolLiteralExpr::False(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        FLtEqExpr::Create(right, left), BoolLiteralExpr::False(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        FLtEqExpr::Create(left, left), BoolLiteralExpr::True(ctx)
    ));

    auto nan = FloatLiteralExpr::Get(fp32Ty, llvm::APFloat::getNaN(fp32Ty.getLLVMSemantics()));
    EXPECT_TRUE(checkBvOp(
        FEqExpr::Create(nan, nan), BoolLiteralExpr::False(ctx)
    ));
}

TEST_F(SolverZ3Test, FloatQuery)
{
    auto& fp32Ty = FloatType::Get(ctx, FloatType::Single);

    auto x = FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(1.5f));
    auto nan = FloatLiteralExpr::Get(fp32Ty, llvm::APFloat::getNaN(fp32Ty.getLLVMSemantics()));
    auto plusInf = FloatLiteralExpr::Get(fp32Ty, llvm::APFloat::getInf(fp32Ty.getLLVMSemantics(), false));
    auto negInf = FloatLiteralExpr::Get(fp32Ty, llvm::APFloat::getInf(fp32Ty.getLLVMSemantics(), true));

    EXPECT_TRUE(checkBvOp(
        FIsNanExpr::Create(nan), BoolLiteralExpr::True(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        FIsNanExpr::Create(x), BoolLiteralExpr::False(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        FIsNanExpr::Create(plusInf), BoolLiteralExpr::False(ctx)
    ));

    EXPECT_TRUE(checkBvOp(
        FIsInfExpr::Create(nan), BoolLiteralExpr::False(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        FIsInfExpr::Create(plusInf), BoolLiteralExpr::True(ctx)
    ));
    EXPECT_TRUE(checkBvOp(
        FIsInfExpr::Create(negInf), BoolLiteralExpr::True(ctx)
    ));
}

TEST_F(SolverZ3Test, BvToFpCasts)
{
    auto& fp32Ty = FloatType::Get(ctx, FloatType::Single);
    auto& fp64Ty = FloatType::Get(ctx, FloatType::Double);

    std::array singlePrecisionInts{0, 1, 2, 5, 10, 20, 100, 200, 500};

    // See semantic transformation with floats
    for (int intVal : singlePrecisionInts) {
        auto bvLit = BvLiteralExpr::Get(BvType::Get(ctx, 32), llvm::APInt(32, intVal, true));
        auto fp32Lit = FloatLiteralExpr::Get(fp32Ty, llvm::APFloat((float) intVal));

        EXPECT_TRUE(checkBvOp(SignedToFpExpr::Create(bvLit, fp32Ty, llvm::APFloat::rmNearestTiesToEven), fp32Lit));
        EXPECT_TRUE(checkBvOp(UnsignedToFpExpr::Create(bvLit, fp32Ty, llvm::APFloat::rmNearestTiesToEven), fp32Lit));
    }

    // Now with the double type
    for (int intVal : singlePrecisionInts) {
        auto bvLit = BvLiteralExpr::Get(BvType::Get(ctx, 64), llvm::APInt(64, intVal, true));
        auto fp64Lit = FloatLiteralExpr::Get(fp64Ty, llvm::APFloat((double) intVal));

        EXPECT_TRUE(checkBvOp(SignedToFpExpr::Create(bvLit, fp64Ty, llvm::APFloat::rmNearestTiesToEven), fp64Lit));
        EXPECT_TRUE(checkBvOp(UnsignedToFpExpr::Create(bvLit, fp64Ty, llvm::APFloat::rmNearestTiesToEven), fp64Lit));
    }
}

TEST_F(SolverZ3Test, BvBitcastToFloat)
{
    auto& fp32Ty = FloatType::Get(ctx, FloatType::Single);
    auto& bv32Ty = BvType::Get(ctx, 32);

    EXPECT_TRUE(
        checkBvOp(BvToFpExpr::Create(BvLiteralExpr::Get(bv32Ty, 0x7F800000), fp32Ty), FloatLiteralExpr::Get(fp32Ty, llvm::APFloat::getInf(fp32Ty.getLLVMSemantics(), false))));
    EXPECT_TRUE(
        checkBvOp(BvToFpExpr::Create(BvLiteralExpr::Get(bv32Ty, 0xFF800000), fp32Ty), FloatLiteralExpr::Get(fp32Ty, llvm::APFloat::getInf(fp32Ty.getLLVMSemantics(), true))));

    EXPECT_TRUE(
        checkBvOp(BvToFpExpr::Create(BvLiteralExpr::Get(bv32Ty, 0x80000000), fp32Ty), FloatLiteralExpr::Get(fp32Ty, llvm::APFloat::getZero(fp32Ty.getLLVMSemantics(), true))));
    EXPECT_TRUE(
        checkBvOp(BvToFpExpr::Create(BvLiteralExpr::Get(bv32Ty, 0x00000000), fp32Ty), FloatLiteralExpr::Get(fp32Ty, llvm::APFloat::getZero(fp32Ty.getLLVMSemantics(), false))));
}

TEST_F(SolverZ3Test, FpToBvCasts)
{
    auto& fp32Ty = FloatType::Get(ctx, FloatType::Single);
    auto& bv32Ty = BvType::Get(ctx, 32);

    EXPECT_TRUE(checkBvOp(
        FpToSignedExpr::Create(FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(fp32Ty.getLLVMSemantics(), 1.0f)), bv32Ty, llvm::APFloat::rmNearestTiesToEven),
        BvLiteralExpr::Get(bv32Ty, 1)
    ));
    EXPECT_TRUE(checkBvOp(
        FpToUnsignedExpr::Create(FloatLiteralExpr::Get(fp32Ty, llvm::APFloat(fp32Ty.getLLVMSemantics(), 1.0f)), bv32Ty, llvm::APFloat::rmNearestTiesToEven),
        BvLiteralExpr::Get(bv32Ty, 1)
    ));

    // See bit-cast to bv32
    EXPECT_TRUE(checkBvOp(
        FpToBvExpr::Create(FloatLiteralExpr::Get(fp32Ty, llvm::APFloat::getInf(fp32Ty.getLLVMSemantics(), false)), bv32Ty),
        BvLiteralExpr::Get(bv32Ty, 0x7F800000)
    ));
    EXPECT_TRUE(checkBvOp(
        FpToBvExpr::Create(FloatLiteralExpr::Get(fp32Ty, llvm::APFloat::getInf(fp32Ty.getLLVMSemantics(), true)), bv32Ty),
        BvLiteralExpr::Get(bv32Ty, 0xFF800000)
    ));
}

TEST_F(SolverZ3Test, Arrays)
{
    // Example from the Z3 tutorial:
    //  (declare-const x Int)
    //  (declare-const y Int)
    //  (declare-const a1 (Array Int Int))
    //  (assert (= (select a1 x) x))
    //  (assert (= (store a1 x y) a1))
    auto x = ctx.createVariable("x", IntType::Get(ctx));
    auto y = ctx.createVariable("y", IntType::Get(ctx));
    auto a1 = ctx.createVariable("a1", ArrayType::Get(IntType::Get(ctx), IntType::Get(ctx)));

    solver->add(
        EqExpr::Create(ArrayReadExpr::Create(a1->getRefExpr(), x->getRefExpr()), x->getRefExpr()));
    solver->add(EqExpr::Create(
        ArrayWriteExpr::Create(a1->getRefExpr(), x->getRefExpr(), y->getRefExpr()),
        a1->getRefExpr()));

    auto result = solver->run();
    ASSERT_EQ(result, Solver::SAT);

    solver->add(NotEqExpr::Create(x->getRefExpr(), y->getRefExpr()));
    result = solver->run();
    ASSERT_EQ(result, Solver::UNSAT);
}

TEST_F(SolverZ3Test, ArrayLiterals)
{
    auto a1 = ArrayLiteralExpr::Get(
        ArrayType::Get(IntType::Get(ctx), IntType::Get(ctx)),
        {{IntLiteralExpr::Get(ctx, 1), IntLiteralExpr::Get(ctx, 1)},
         {IntLiteralExpr::Get(ctx, 2), IntLiteralExpr::Get(ctx, 2)}},
        IntLiteralExpr::Get(ctx, 0));

    auto v = ctx.createVariable("v", ArrayType::Get(IntType::Get(ctx), IntType::Get(ctx)));

    solver->add(EqExpr::Create(a1, v->getRefExpr()));
    auto result = solver->run();

    ASSERT_EQ(result, Solver::SAT);
    ASSERT_EQ(solver->getModel()->evaluate(v->getRefExpr()), a1);
}

TEST_F(SolverZ3Test, Tuples)
{
    // Example from the Z3 tutorial:
    //  (declare-const p1 (Pair Int Int))
    //  (declare-const p2 (Pair Int Int))
    //  (assert (= p1 p2))
    //  (assert (> (second p1) 20))
    //  (check-sat)
    //  (assert (not (= (first p1) (first p2))))
    //  (check-sat)
    auto& intTy = IntType::Get(ctx);
    auto& tupTy = TupleType::Get(intTy, intTy);

    auto p1 = ctx.createVariable("p1", tupTy)->getRefExpr();
    auto p2 = ctx.createVariable("p2", tupTy)->getRefExpr();

    solver->add(EqExpr::Create(p1, p2));
    solver->add(GtExpr::Create(TupleSelectExpr::Create(p1, 1), IntLiteralExpr::Get(ctx, 20)));

    auto status = solver->run();
    EXPECT_EQ(status, Solver::SAT);

    solver->add(NotEqExpr::Create(TupleSelectExpr::Create(p1, 0), TupleSelectExpr::Create(p2, 0)));

    status = solver->run();
    EXPECT_EQ(status, Solver::UNSAT);
}

TEST_F(SolverZ3Test, TuplesConstruct)
{
    auto& intTy = IntType::Get(ctx);
    auto& tupTy = TupleType::Get(intTy, intTy);

    auto p1 = ctx.createVariable("p1", tupTy)->getRefExpr();
    auto p2 = ctx.createVariable("p2", tupTy)->getRefExpr();
    auto x = ctx.createVariable("x", intTy)->getRefExpr();

    solver->add(EqExpr::Create(p1, p2));
    solver->add(
        EqExpr::Create(p2, TupleConstructExpr::Create(tupTy, IntLiteralExpr::Get(intTy, 2), x)));
    solver->add(EqExpr::Create(x, IntLiteralExpr::Get(intTy, 5)));

    auto status = solver->run();
    ASSERT_EQ(status, Solver::SAT);

    auto model = solver->getModel();
    // ASSERT_EQ(model->evaluate(TupleSelectExpr::Create(p1, 0)), IntLiteralExpr::Get(intTy, 2));
    // ASSERT_EQ(model->evaluate(TupleSelectExpr::Create(p1, 1)), IntLiteralExpr::Get(intTy, 5));
}

TEST_F(SolverZ3Test, Reset)
{
    auto x = ctx.createVariable("x", IntType::Get(ctx))->getRefExpr();
    auto y = ctx.createVariable("y", IntType::Get(ctx))->getRefExpr();

    auto xEq1 = EqExpr::Create(x, IntLiteralExpr::Get(IntType::Get(ctx), 1));
    auto yEq2 = EqExpr::Create(y, IntLiteralExpr::Get(IntType::Get(ctx), 2));

    solver->add(xEq1);
    solver->add(yEq2);

    auto status = solver->run();
    ASSERT_EQ(status, Solver::SAT);

    solver->add(EqExpr::Create(x, y));
    status = solver->run();
    ASSERT_EQ(status, Solver::UNSAT);

    EXPECT_EQ(solver->getNumConstraints(), 3);

    solver->reset();
    ASSERT_EQ(solver->getNumConstraints(), 0);

    solver->add(xEq1);
    status = solver->run();
    ASSERT_EQ(status, Solver::SAT);
}

TEST_F(SolverZ3Test, PushPop)
{
    auto x = ctx.createVariable("x", IntType::Get(ctx))->getRefExpr();
    auto y = ctx.createVariable("y", IntType::Get(ctx))->getRefExpr();

    auto xEq1 = EqExpr::Create(x, IntLiteralExpr::Get(IntType::Get(ctx), 1));
    auto yEq2 = EqExpr::Create(y, IntLiteralExpr::Get(IntType::Get(ctx), 2));

    solver->push();

    solver->add(xEq1);
    solver->add(yEq2);
    EXPECT_EQ(solver->getNumConstraints(), 2);

    auto status = solver->run();
    ASSERT_EQ(status, Solver::SAT);

    solver->push();

    solver->add(EqExpr::Create(x, y));
    EXPECT_EQ(solver->getNumConstraints(), 3);

    status = solver->run();
    ASSERT_EQ(status, Solver::UNSAT);

    solver->pop();
    EXPECT_EQ(solver->getNumConstraints(), 2);

    status = solver->run();
    ASSERT_EQ(status, Solver::SAT);

    solver->pop();
    EXPECT_EQ(solver->getNumConstraints(), 0);
}

TEST_F(SolverZ3Test, Dump)
{
    std::vector<std::string> expectedParts = {
        "(declare-fun x () Int)",
        "(declare-fun y () Int)",
        "(assert (= x y))",
        "(assert (= y 5))",
    };

    std::string actual;
    llvm::raw_string_ostream rso(actual);

    auto x = ctx.createVariable("x", IntType::Get(ctx))->getRefExpr();
    auto y = ctx.createVariable("y", IntType::Get(ctx))->getRefExpr();

    solver->add(EqExpr::Create(x, y));
    solver->add(EqExpr::Create(y, IntLiteralExpr::Get(ctx, 5)));

    solver->dump(rso);
    rso.flush();

    // There might be other stuff in the string in any order, just search for the important part
    for (auto& expected : expectedParts) {
        ASSERT_TRUE(actual.find(expected) != std::string::npos);
    }
}

} // namespace
