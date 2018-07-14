#include "gazer/Core/Utils/CfaSimplify.h"

#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/SmallVector.h>

using namespace gazer;
using llvm::dyn_cast;

static bool isSequentialLocation(const Location& loc)
{
    return loc.getNumIncoming() == 1 && loc.getNumOutgoing() == 1
        && (*loc.outgoing_begin())->getGuard() == nullptr
        && (*loc.incoming_begin())->getKind() == CfaEdge::Edge_Assign
        && (*loc.outgoing_begin())->getKind() == CfaEdge::Edge_Assign;
        //&& (&loc.getParent()->entry())) != &loc
        //&& (&loc.getParent()->exit()) != &loc
        //&& loc.getName() != "error";
}

static bool isSingleOutgoingLocation(const Location& loc)
{
    return loc.getNumOutgoing() == 1
        && (*loc.outgoing_begin())->getGuard() == nullptr
        && std::all_of(loc.incoming_begin(), loc.incoming_end(), [](CfaEdge* edge) {
            return edge->getKind() == CfaEdge::Edge_Assign;
        })
        && (*loc.outgoing_begin())->getKind() == CfaEdge::Edge_Assign
        && (&loc.getParent()->entry() != &loc)
        && (&loc.getParent()->exit() != &loc)
        && (loc.getName() != "error");
}

void gazer::SimplifyCFA(Automaton& cfa)
{
    bool changed = true;
    llvm::DenseSet<Location*> locsToRemove;

    // Find sequential edges
    while (changed) {
        changed = false;
        for (auto it = cfa.loc_begin(); it != cfa.loc_end(); ++it) {
            std::unique_ptr<Location>& loc = *it;
            if (isSingleOutgoingLocation(*loc)) {
                llvm::SmallVector<CfaEdge*, 2> incomingEdges(loc->incoming_begin(), loc->incoming_end());

                // This is an unneeded location.
                // The AssignEdge cast is safe because we checked the edge type
                // in the condition.
                AssignEdge* outgoing = dyn_cast<AssignEdge>(*loc->outgoing_begin());
                for (CfaEdge* incomingEdge : incomingEdges) {
                    AssignEdge* incoming = dyn_cast<AssignEdge>(incomingEdge);
                    
                    // Create a single sequential edge between the incoming source
                    // and the outgoing target.
                    std::vector<AssignEdge::Assignment> assigns(
                        incoming->assign_begin(), incoming->assign_end()
                    );
                    assigns.insert(assigns.end(), outgoing->assign_begin(), outgoing->assign_end());

                    cfa.insertEdge(AssignEdge::Create(
                        incoming->getSource(), outgoing->getTarget(), assigns, incoming->getGuard()
                    ));

                    cfa.removeEdge(incomingEdge);
                }
                cfa.removeEdge(outgoing);

                locsToRemove.insert(loc.get());
                changed = true;
            }
        }
    }

    for (Location* loc : locsToRemove) {
        cfa.removeLocation(loc);
    }
}
