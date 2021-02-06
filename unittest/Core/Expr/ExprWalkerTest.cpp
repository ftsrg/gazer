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
#include "gazer/Core/Expr/ExprWalker.h"
#include "gazer/Core/Expr/ExprBuilder.h"

#include <llvm/Support/raw_ostream.h>

#include <gtest/gtest.h>

using namespace gazer;

namespace
{

class PrintKindWalker : public ExprWalker<void*>
{
public:
    void* print(const ExprPtr& expr) {
        return this->walk(expr);
    }

protected:
    void* visitExpr(const ExprPtr& expr) override
    {
        Res += Expr::getKindName(expr->getKind()).str() + " ";
        return nullptr;
    }
    
    void* visitVarRef(const ExprRef<VarRefExpr>& expr) override
    {
        Res += expr->getVariable().getName() + " ";
        return nullptr;
    }

public:
    std::string Res;
};

TEST(ExprWalkerTest, TestTraversal)
{
    GazerContext context;
    auto builder = CreateExprBuilder(context);

    auto eq = builder->Eq(
        context.createVariable("A", BvType::Get(context, 32))->getRefExpr(),
        builder->ZExt(
            context.createVariable("B", BvType::Get(context, 8))->getRefExpr(),
            BvType::Get(context, 32)
        )
    );

    auto expr = builder->Not(
        builder->Imply(
            builder->And(
                context.createVariable("X", BoolType::Get(context))->getRefExpr(),
                eq
            ),
            builder->Or(
                eq,
                context.createVariable("Y", BoolType::Get(context))->getRefExpr()
            )
        )
    );

    PrintKindWalker walker;
    walker.print(expr);

    ASSERT_EQ(walker.Res, "X A B ZExt Eq And A B ZExt Eq Y Or Imply Not ");
}

class PrintAndOperandsWalker : public ExprWalker<std::string>
{
public:
    std::string visit(const ExprPtr& expr) {
        return this->walk(expr);
    }

protected:
    std::string visitAnd(const ExprRef<AndExpr>& expr) override
    {
        std::string buff;
        llvm::raw_string_ostream rso{buff};
        rso << "And(";
        for (size_t i = 0; i < expr->getNumOperands(); ++i) {
            rso << i << ": " << getOperand(i) << " ";
        }
        rso << ")";

        return rso.str();
    }

    std::string visitVarRef(const ExprRef<VarRefExpr>& expr) override
    {
        return expr->getVariable().getName();
    }

    std::string visitExpr(const ExprPtr& expr) override
    {
        return "";
    }
};


TEST(ExprWalkerTest, TestGetOperand)
{
    GazerContext context;
    auto a = context.createVariable("A", BoolType::Get(context));
    auto b = context.createVariable("B", BoolType::Get(context));
    auto c = context.createVariable("C", BoolType::Get(context));
    auto d = context.createVariable("D", BoolType::Get(context));

    auto expr = AndExpr::Create({
        a->getRefExpr(), b->getRefExpr(), c->getRefExpr(), d->getRefExpr()
    });

    PrintAndOperandsWalker walker;
    auto res = walker.visit(expr);

    ASSERT_EQ(res, "And(0: A 1: B 2: C 3: D )");
}

} // end anonymous namespace