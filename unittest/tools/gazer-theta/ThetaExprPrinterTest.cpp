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
#include "../../../tools/gazer-theta/lib/ThetaCfaGenerator.h"

#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/Type.h"

#include <gtest/gtest.h>

using namespace gazer;

namespace
{

class ThetaExprPrinterTest : public ::testing::Test
{
public:
    ThetaExprPrinterTest();

protected:
    GazerContext ctx;
    std::unique_ptr<ExprBuilder> b;
    std::vector<std::pair<ExprPtr, std::string>> tests;
};

ThetaExprPrinterTest::ThetaExprPrinterTest()
    : b(CreateExprBuilder(ctx)),
    tests({
              { b->True(),                                    "true" },
              { b->False(),                                   "false" },
              { b->IntLit(1),                                 "1" },
              { b->Add(b->IntLit(2), b->IntLit(3)),           "(2 + 3)" },
              { b->Sub(b->IntLit(2), b->IntLit(3)),           "(2 - 3)" },
              { b->Mul(b->IntLit(2), b->IntLit(3)),           "(2 * 3)" },
              { b->Div(b->IntLit(2), b->IntLit(3)),           "(2 / 3)" },
              { b->Add(b->IntLit(-2), b->IntLit(3)),          "((-2) + 3)" },
              { b->Sub(b->IntLit(2), b->IntLit(-3)),          "(2 - (-3))" },
              { b->And(b->True(), b->False()),                "(true and false)" },
              { b->Or(b->False(), b->True()),                 "(false or true)" },
              { b->And({b->True(), b->True(), b->False()}),   "(true and true and false)" },
              { b->Or({b->True(), b->True(), b->False()}),    "(true or true or false)" },
              { b->Mul(b->Mul(b->IntLit(1), b->Div(b->IntLit(2), b->IntLit(3))), b->IntLit(4)), "((1 * (2 / 3)) * 4)"},
              { b->And({
                           b->Eq(b->IntLit(1), b->IntLit(1)),
                           b->Lt(b->IntLit(1), b->IntLit(2)),
                           b->Gt(b->IntLit(2), b->IntLit(1)) }),       "((1 = 1) and (1 < 2) and (2 > 1))"
              },
              { b->Select(b->Eq(b->IntLit(1), b->IntLit(2)), b->IntLit(3), b->IntLit(4)), "(if (1 = 2) then 3 else 4)" },
              { b->Add(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvadd 8'b00000011)" },
              { b->Sub(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvsub 8'b00000011)" },
              { b->Mul(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvmul 8'b00000011)" },
              { b->BvUDiv(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvudiv 8'b00000011)" },
              { b->BvSDiv(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvsdiv 8'b00000011)" },
              { b->Mod(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvsmod 8'b00000011)" },
              { b->BvURem(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvurem 8'b00000011)" },
              { b->BvSRem(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvsrem 8'b00000011)" },
              { b->BvAnd(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvand 8'b00000011)" },
              { b->BvOr(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvor 8'b00000011)" },
              { b->BvXor(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvxor 8'b00000011)" },
              { b->Shl(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvshl 8'b00000011)" },
              { b->AShr(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvashr 8'b00000011)" },
              { b->LShr(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvlshr 8'b00000011)" },
              { b->BvConcat(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 ++ 8'b00000011)" },
              { b->Extract(b->BvLit(2, 8), 3, 3), "(8'b00000010)[6:3]" },
              { b->ZExt(b->BvLit(2, 8), BvType::Get(ctx, 16)), "(8'b00000010 bv_zero_extend bv[16])" },
              { b->SExt(b->BvLit(2, 8), BvType::Get(ctx, 16)), "(8'b00000010 bv_sign_extend bv[16])" },
              { b->BvULt(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvult 8'b00000011)" },
              { b->BvULtEq(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvule 8'b00000011)" },
              { b->BvUGt(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvugt 8'b00000011)" },
              { b->BvUGtEq(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvuge 8'b00000011)" },
              { b->BvSLt(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvslt 8'b00000011)" },
              { b->BvSLtEq(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvsle 8'b00000011)" },
              { b->BvSGt(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvsgt 8'b00000011)" },
              { b->BvSGtEq(b->BvLit(2, 8), b->BvLit(3, 8)), "(8'b00000010 bvsge 8'b00000011)" },
              { b->ArrayLit({
                    { b->BvLit(2, 4), b->BvLit(3, 4) },
                    { b->BvLit(1, 4), b->BvLit(0, 4) }
                                                        }, b->BvLit(0, 4)), "[4'b0010 <- 4'b0011, 4'b0001 <- 4'b0000, default <- 4'b0000]" 
              },
              { b->ArrayLit(ArrayType::Get(BvType::Get(ctx, 8), BvType::Get(ctx, 4)), {}, b->BvLit(0, 4)), "[<bv[8]>default <- 4'b0000]" },
              { b->ArrayLit(ArrayType::Get(BvType::Get(ctx, 8), BvType::Get(ctx, 4)), {}), "__gazer_uninitialized_memory___bv_8_bv_4__" }
          })
{}

TEST_F(ThetaExprPrinterTest, TestPrintExpr)
{
    for (auto& [ expr, expected ] : tests) {
        EXPECT_EQ(theta::printThetaExpr(expr), expected);
    }
}

}
