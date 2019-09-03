#include "gazer/Core/Expr/ExprWalker.h"
#include "gazer/Core/Expr/ExprBuilder.h"

#include <llvm/Support/raw_ostream.h>

#include <gtest/gtest.h>

using namespace gazer;

namespace
{

class PrintKindWalker : public ExprWalker<PrintKindWalker, void*>
{
public:
    void* visitExpr(const ExprPtr& expr)
    {
        Res += Expr::getKindName(expr->getKind()).str() + " ";
        return nullptr;
    }
    
    void* visitVarRef(const ExprRef<VarRefExpr>& expr)
    {
        Res += expr->getVariable().getName() + " ";
        return nullptr;
    }

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
    walker.visit(expr);

    ASSERT_EQ(walker.Res, "X A B ZExt Eq And A B ZExt Eq Y Or Imply Not ");
}

class PrintAndOperandsWalker : public ExprWalker<PrintAndOperandsWalker, std::string>
{
public:
    std::string visitAnd(const ExprRef<AndExpr>& expr)
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

    std::string visitVarRef(const ExprRef<VarRefExpr>& expr)
    {
        return expr->getVariable().getName();
    }

    std::string visitExpr(const ExprPtr& expr)
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