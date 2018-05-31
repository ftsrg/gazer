#include "gazer/Core/Automaton.h"

#include <llvm/ADT/GraphTraits.h>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <iostream>

using namespace gazer;

bool Location::operator==(const Location& rhs) const{
    if (getParent() != rhs.getParent()) {
        return false;
    } else if (getName() != rhs.getName()) {
        return false;
    }

    return true;
}

bool CfaEdge::operator==(const CfaEdge& rhs) const {
    return getSource() == rhs.getSource() && getTarget() == rhs.getTarget();
}

std::ostream& gazer::operator<<(std::ostream& os, const Location& location)
{
    return os << location.getName();
}

std::ostream& gazer::operator<<(std::ostream& os, const CfaEdge& edge)
{
    return os << fmt::format("({0},{1})", edge.getSource(), edge.getTarget());
}

void AssignEdge::print(std::ostream& os) const
{
    if (getGuard() != nullptr) {
        os << "[ ";
        getGuard()->print(os);
        os << " ]\\n";
    }

    for (auto& assign : mAssignments) {
        os << assign.variable.getName() << " := ";
        assign.expr->print(os);
        os << "\\n";
    }
}

Location& Automaton::createLocation(std::string name)
{
    auto& ptr = mLocs.emplace_back(new Location(name, this));
    return *ptr;
}

CfaEdge& Automaton::insertEdge(std::unique_ptr<CfaEdge> edge)
{
    std::cerr << *edge << std::endl;

    edge->getSource().addOutgoing(edge.get());
    edge->getTarget().addIncoming(edge.get());

    mEdges.push_back(std::move(edge));

    return *edge;
}
