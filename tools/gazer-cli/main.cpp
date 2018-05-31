#include "gazer/Core/Automaton.h"
#include "gazer/Core/SymbolTable.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/Variable.h"

#include "gazer/Core/Solver/Solver.h"
#include "gazer/Z3Solver/Z3Solver.h"

#include "gazer/BMC/BMC.h"
#include "gazer/Core/Utils/CfaUtils.h"

#include <fmt/format.h>

#include <sstream>
#include <iostream>

using namespace gazer;

static void printAutomaton(Automaton& cfa);

int main(int argc, char* argv[])
{
    Automaton cfa("entry", "exit");
    Location& loc1 = cfa.createLocation();
    Location& loc2 = cfa.createLocation();
    Location& loc3 = cfa.createLocation();
    Location& err  = cfa.createLocation("error");

    Variable& x = cfa.getSymbols().create("x", IntType::get(32));
    Variable& y = cfa.getSymbols().create("y", IntType::get(32));
    Variable& c = cfa.getSymbols().create("c", BoolType::get());

    auto condition = LtExpr::Create(
        y.getRefExpr(), IntLiteralExpr::get(*IntType::get(32), 10)
    );
    //auto condition = BoolLiteralExpr::getTrue();

    auto zero = IntLiteralExpr::get(*IntType::get(32), 0);

    cfa.insertEdge(AssignEdge::Create(cfa.entry(), loc1, {
        {x, zero},
      //  {y, UndefExpr::Get(*IntType::get(32))}
        {y, zero}
    }));
    cfa.insertEdge(AssumeEdge::Create(loc1, loc2, condition));
    cfa.insertEdge(AssignEdge::Create(loc2, loc1, {
        {x, AddExpr::Create(x.getRefExpr(), y.getRefExpr()) },
        {y, AddExpr::Create(y.getRefExpr(), IntLiteralExpr::get(*IntType::get(32), 1)) }
    }));
    cfa.insertEdge(AssumeEdge::Create(loc1, loc3, NotExpr::Create(condition)));

    auto assertCond = EqExpr::Create(
        y.getRefExpr(), zero
    );

    cfa.insertEdge(AssumeEdge::Create(loc3, cfa.exit(), assertCond));
    cfa.insertEdge(AssumeEdge::Create(loc3, err, NotExpr::Create(assertCond)));

    Z3Solver solver;
    BoundedModelChecker bmc([](Location* location) {
        return location->getName() == "error";
    }, 100, &solver);
    auto status = bmc.check(cfa);

    if (status == BoundedModelChecker::STATUS_UNSAFE) {
        std::cerr << "UNSAFE\n";
    } else {
        std::cerr << "UNKNOWN\n";
    }


    return 0;
}

static void printAutomaton(Automaton& cfa)
{
    std::cerr << "digraph G {\n";
    for (auto& loc : cfa.locs()) {
        std::cerr << fmt::format("\tnode_{0} [label=\"{1}\"];\n",
            static_cast<void*>(loc.get()), loc->getName()
        );
    }
    std::cerr << "\n";
    for (auto& edge : cfa.edges()) {
        std::stringstream ss;
        edge->print(ss);

        std::cerr << fmt::format("\tnode_{0} -> node_{1} [label=\"{2}\"];\n",
            static_cast<void*>(&edge->getSource()),
            static_cast<void*>(&edge->getTarget()),
            ss.str()
        );
    }
    std::cerr << "};\n";
}
