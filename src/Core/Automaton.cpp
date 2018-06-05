#include "gazer/Core/Automaton.h"

#include <llvm/ADT/GraphTraits.h>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <iostream>
#include <algorithm>

using namespace gazer;

bool Location::operator==(const Location& rhs) const{
    if (getParent() != rhs.getParent()) {
        return false;
    } else if (getName() != rhs.getName()) {
        return false;
    }

    return true;
}

void Location::removeIncoming(CfaEdge* edge) {
    mIncoming.erase(
        std::remove(mIncoming.begin(), mIncoming.end(), edge)
    );
}

void Location::removeOutgoing(CfaEdge* edge) {
    mOutgoing.erase(
        std::remove(mOutgoing.begin(), mOutgoing.end(), edge)  
    );
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
        os << assign.variable->getName() << " := ";
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
    edge->getSource().addOutgoing(edge.get());
    edge->getTarget().addIncoming(edge.get());

    mEdges.push_back(std::move(edge));

    return *edge;
}

void Automaton::removeEdge(CfaEdge* edge)
{
    edge->getSource().removeOutgoing(edge);
    edge->getTarget().removeIncoming(edge);

    auto pos = std::remove_if(mEdges.begin(), mEdges.end(), [edge](auto& ptr) {
        return ptr.get() == edge;
    });
    mEdges.erase(pos, mEdges.end());
}

void Automaton::removeLocation(Location* location)
{
    assert(location->getNumIncoming() == 0 && "Can only remove orphaned locations");
    assert(location->getNumOutgoing() == 0 && "Can only remove orphaned locations");

    auto pos = std::remove_if(mLocs.begin(), mLocs.end(), [location](auto& ptr) {
        return ptr.get() == location;
    });
    mLocs.erase(pos, mLocs.end());
}


