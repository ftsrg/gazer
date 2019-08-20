#include "gazer/Automaton/Cfa.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/Support/raw_ostream.h>

#include <gtest/gtest.h>

using namespace gazer;

TEST(CfaPrinter, TestPrint)
{
    GazerContext ctx;
    AutomataSystem system{ctx};

    auto calc = system.createCfa("calc");
    calc->createInput("a", BvType::Get(ctx, 32));

    auto q = calc->createLocal("q", BvType::Get(ctx, 32));
    calc->addOutput(q);

    auto cfa = system.createCfa("main");
    auto x = cfa->createInput("x", BvType::Get(ctx, 32));
    auto y = cfa->createInput("y", BvType::Get(ctx, 32));

    auto ret = cfa->createLocal("RET_VAL", BvType::Get(ctx, 32));
    cfa->addOutput(ret);

    auto m1 = cfa->createLocal("m1", BvType::Get(ctx, 32));
    auto m2 = cfa->createLocal("m2", BvType::Get(ctx, 32));

    auto l1 = cfa->createLocation();
    auto l2 = cfa->createLocation();
    auto l3 = cfa->createLocation();
    auto lr = cfa->createErrorLocation();

    cfa->createAssignTransition(cfa->getEntry(), l1);
    cfa->createAssignTransition(l1, l2, BoolLiteralExpr::True(ctx), {
        VariableAssignment{
            m1, AddExpr::Create(x->getRefExpr(), y->getRefExpr())
        }
    });
    cfa->createCallTransition(l2, l3, calc, { m1->getRefExpr() }, {
        { m2, q->getRefExpr() }
    });

    cfa->print(llvm::errs());
}
