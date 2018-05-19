#include "gazer/Core/Automaton.h"

#include <gtest/gtest.h>

using namespace gazer;

TEST(Automaton, CanCreateEmptyAutomaton)
{
    Automaton cfa("entry", "exit");
    ASSERT_EQ(cfa.entry().getName(), "entry");
    ASSERT_EQ(cfa.entry().getParent(), &cfa);

    ASSERT_EQ(cfa.exit().getName(), "exit");
    ASSERT_EQ(cfa.exit().getParent(), &cfa);
}

/*
TEST(Automaton, CanInsertLocationsAndEdges)
{
    Automaton cfa("entry", "exit");
    Location* loc1 = cfa.insert("loc1");
    Location* loc2 = cfa.insert("loc2");

    ASSERT_EQ(loc1.getName(), "loc1");
    ASSERT_EQ(loc1.getParent(), &cfa);

    ASSERT_EQ(loc2.getName(), "loc2");
    ASSERT_EQ(loc2.getParent(), &cfa);

    cfa.addEdge(cfa.entry(), loc1);
    cfa.addEdge(loc1, cfa.exit());
    //cfa.addEdge(loc1, loc2);
    //cfa.addEdge(loc2, cfa.exit());

    for (auto& edge : cfa.entry().outgoing()) {
        std::cout << *edge << " ";
    }

    CfaEdge* entryOut = *(cfa.entry().outgoing_begin());
    CfaEdge* loc1In = *(loc1->incoming_begin());

    ASSERT_EQ(*entryOut, *loc1In);

    std::cout << entryOut << " " << loc1In;

    ASSERT_EQ(loc1In.getTarget(), loc1);
    ASSERT_EQ(loc1In.getSource(), cfa.entry());
}
*/