/// \file Printing utilities for control flow automata.
#include "gazer/Automaton/Cfa.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Expr/ExprUtils.h"
#include "gazer/ADT/StringUtils.h"

#include <llvm/ADT/StringExtras.h>

#include <llvm/Support/DOTGraphTraits.h>
#include <llvm/Support/GraphWriter.h>
#include <llvm/Support/Format.h>

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
            for (const VariableAssignment& varAssignment : *assign) {
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
            for (size_t i = 0; i < call->getNumInputs(); ++i) {
                os << call->getCalledAutomaton()->getInput(i)->getName()
                << " := " << *call->getInputArgument(i) << ", ";
            }
            os << ") -> {";
            for (const VariableAssignment& varAssignment : call->outputs()) {
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

void Cfa::print(llvm::raw_ostream& os) const
{
    constexpr auto indent1 = "    ";
    constexpr auto indent2 = "        ";

    auto printIo = [](llvm::raw_ostream& os, Variable* variable) {
        os << variable->getName() << " : " << variable->getType();
    };

    auto printCallInput = [](llvm::raw_ostream& os, const ExprPtr& expr) {
        InfixPrintExpr(expr, os);
    };

    auto printCallOutput = [](llvm::raw_ostream& os, const VariableAssignment& assign) {
        os << assign.getVariable()->getName() << " <= ";
        InfixPrintExpr(assign.getValue(), os);
    };

    os << "procedure " << mName << "(";
    join_print_as(os, mInputs.begin(), mInputs.end(), ", ", printIo);
    os << ") -> (";
    join_print_as(os, mOutputs.begin(), mOutputs.end(), ", ", printIo);
    os << ")\n";

    os << "{\n";

    for (auto& local : mLocals) {
        os << indent1 << "var "
        << local->getName() << " : " << local->getType()
        << "\n";
    }

    os << "\n";

    for (auto& loc : mLocations) {
        os << indent1 << "loc $" << loc->getId();
        if (loc->isError()) {
            os << " error";
        }

        if (mEntry == loc.get()) {
            os << " entry ";
        }

        if (mExit == loc.get()) {
            os << " final ";
        }

        os << "\n";
    }

    os << "\n";

    for (auto& edge : mTransitions) {
        os << indent1 << "transition "
        << "$" << edge->getSource()->getId()
        << " -> "
        << "$" << edge->getTarget()->getId() << "\n"
        << indent2 << "assume ";
        InfixPrintExpr(edge->getGuard(), os);
        os << "\n";

        if (auto assignEdge = llvm::dyn_cast<AssignTransition>(edge.get())) {
            os << indent1 << "{\n";
            for (auto& assign : *assignEdge) {
                os << indent2 << assign.getVariable()->getName()
                << " := ";
                InfixPrintExpr(assign.getValue(), os);
                os << ";\n";
            }
            os << indent1 << "};\n";
        } else if (auto call = llvm::dyn_cast<CallTransition>(edge.get())) {
            os << indent2 << "call " << call->getCalledAutomaton()->getName() << "(";
            join_print_as(os, call->input_begin(), call->input_end(), ", ", printCallInput);
            if (call->getNumInputs() != 0 && call->getNumOutputs() != 0) {
                os << ", ";
            }
            join_print_as(os, call->output_begin(), call->output_end(), ", ", printCallOutput);
            os << ");\n";            
        } else {
            llvm_unreachable("Unknown transition kind.");
        }

        os << "\n";
    }

    os << "}\n";
}

void AutomataSystem::print(llvm::raw_ostream& os) const
{
    for (auto& cfa : mAutomata) {
        cfa->print(os);
        os << "\n";
    }
}
