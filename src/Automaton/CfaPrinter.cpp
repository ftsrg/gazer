/// \file Printing utilities for control flow automata.
#include "gazer/Automaton/Cfa.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/Support/DOTGraphTraits.h>
#include <llvm/Support/GraphWriter.h>
#include <llvm/Support/Format.h>
#include <llvm/ADT/StringExtras.h>

using namespace gazer;

namespace llvm
{

template<>
struct DOTGraphTraits<Cfa> : public DefaultDOTGraphTraits
{
    using GT = GraphTraits<Cfa>;
    using EdgeIter = typename GT::ChildIteratorType;

    DOTGraphTraits(bool simple = false)
        : DefaultDOTGraphTraits(simple)
    {}

    static std::string getNodeLabel(const Location* loc, const Cfa& cfa)
    {
        std::string id = std::to_string(loc->getId());
        if (loc == cfa.getEntry()) {
            return "entry (" + id + ")";
        }

        if (loc == cfa.getExit()) {
            return "exit (" + id + ")";
        }

        return id;
    }

    static std::string getNodeAttributes(const Location* loc, const Cfa& cfa)
    {
        std::string str;
        llvm::raw_string_ostream rso(str);

        if (loc->isError()) {
            rso << "fillcolor=\"red\",style=filled";
        }

        rso.flush();
        return str;
    }

    static std::string getEdgeAttributes(const Location* loc, EdgeIter edgeIt, const Cfa& cfa)
    {
        std::string str;
        llvm::raw_string_ostream rso(str);

        const Location* target = *edgeIt;

        const Transition* edge = nullptr;
        for (const Transition* ei : loc->outgoing()) {
            if (ei->getTarget() == target) {
                edge = ei;
                break;
            }
        }

        assert(edge != nullptr);

        rso << "label=\"";
        edge->print(rso);
        rso << "\"";

        rso.flush();
        return str;
    }

    static std::string getGraphName(const Cfa& cfa) {
        return "CFA for " + cfa.getName().str();
    }
};

} // end namespace llvm

void Cfa::view() const
{
    llvm::ViewGraph(*this, "CFA");
}

void Transition::print(llvm::raw_ostream &os) const
{
    // Only write the guard condition if it's not trivial
    if (mExpr != BoolLiteralExpr::True(mExpr->getContext())) {
        os << "[" << *mExpr << "]\n";
    }

    switch (mEdgeKind) {
        case Edge_Assign:
        {
            auto assign = llvm::cast<AssignTransition>(this);
            for (VariableAssignment varAssignment : *assign) {
                os
                    << varAssignment.getVariable()->getName()
                    << " := ";
                varAssignment.getValue()->print(os);
                os << "\n";
            }
            return;
        }
        case Edge_Call:
        {
            auto call = llvm::cast<CallTransition>(this);

            os
                << "Call "
                << call->getCalledAutomaton()->getName()
                << "(";
            for (ExprPtr expr : call->inputs()) {
                expr->print(os);
                os << ", ";
            }
            os << ") -> {";
            for (VariableAssignment varAssignment : call->outputs()) {
                os
                    << varAssignment.getVariable()->getName()
                    << " <= ";
                varAssignment.getValue()->print(os);
                os << ", ";
            }
            os << "}\n";

            return;
        }
    }

    llvm_unreachable("Unknown edge kind in Transition::print()!");
}


