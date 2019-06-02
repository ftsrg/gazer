#include "gazer/Automaton/Cfa.h"
#include <gtest/gtest.h>
#include <gazer/Core/LiteralExpr.h>
#include <gazer/Core/ExprTypes.h>

using namespace gazer;

TEST(Cfa, CanCreateCfa)
{
    GazerContext context;
    AutomataSystem system(context);

    auto cfa = system.createCfa("Test");

    // We should already have an entry and exit location at this point
    ASSERT_EQ(2, cfa->getNumLocations());
    ASSERT_NE(nullptr, cfa->getEntry());
    ASSERT_NE(nullptr, cfa->getExit());

    // Add some more locations (0 and 1 are reserved for entry and exit).
    Location* loc2 = cfa->createLocation();
    Location* loc3 = cfa->createLocation();
    Location* loc4 = cfa->createLocation();

    ASSERT_EQ(5, cfa->getNumLocations());

    ASSERT_EQ(2, loc2->getId());
    ASSERT_EQ(3, loc3->getId());
    ASSERT_EQ(4, loc4->getId());

    // Add variables
    Variable* in1 = cfa->createInput("in1", BoolType::Get(context));
    Variable* tmp = cfa->createLocal("tmp", BoolType::Get(context));
    Variable* out1 = cfa->createLocal("out1", BoolType::Get(context));

    cfa->addOutput(out1);

    ASSERT_EQ(1, cfa->getNumInputs());
    ASSERT_EQ(1, cfa->getNumOutputs());
    ASSERT_EQ(2, cfa->getNumLocals());

    ASSERT_EQ("Test/in1", in1->getName());
    ASSERT_EQ("Test/out1", out1->getName());
    ASSERT_EQ("Test/tmp", tmp->getName());

    // Add some edges
    AssignTransition* edge1 = cfa->createAssignTransition(
        cfa->getEntry(), loc2,
        in1->getRefExpr(), {}
    );
    AssignTransition* edge2 = cfa->createAssignTransition(
        cfa->getEntry(), loc3,
        NotExpr::Create(in1->getRefExpr()), {}
    );

    ASSERT_EQ(2, cfa->getNumTransitions());
    ASSERT_EQ(2, cfa->getEntry()->getNumOutgoing());
    ASSERT_EQ(1, loc2->getNumIncoming());
    ASSERT_EQ(1, loc3->getNumIncoming());

    ASSERT_EQ(cfa->getEntry(), edge1->getSource());
    ASSERT_EQ(cfa->getEntry(), edge2->getSource());
    ASSERT_EQ(loc2, edge1->getTarget());
    ASSERT_EQ(loc3, edge2->getTarget());
}
