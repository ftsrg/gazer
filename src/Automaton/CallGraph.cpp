#include "gazer/Automaton/CallGraph.h"
#include "gazer/Automaton/Cfa.h"

#include <llvm/Support/DOTGraphTraits.h>
#include <llvm/Support/raw_ostream.h>

using namespace gazer;

CallGraph::CallGraph(AutomataSystem& system)
{
    // Create a call graph node for every procedure
    for (Cfa& cfa : system) {
        mNodes.try_emplace(&cfa, std::make_unique<Node>(&cfa));
    }

    for (Cfa& cfa : system) {
        auto& node = mNodes[&cfa];

        for (auto& edge : cfa.edges()) {
            if (auto call = llvm::dyn_cast<CallTransition>(edge.get())) {
                node->addCall(call, mNodes[call->getCalledAutomaton()].get());
            }
        }
    }
}

bool CallGraph::isTailRecursive(Cfa* cfa)
{
    auto& node = mNodes[cfa];

    bool isRecursive = false;
    bool isTail = true;

    for (auto& call : node->mCalls) {
        if (call.first->getCalledAutomaton() == cfa) {
            isRecursive = true;
            if (call.first->getTarget() != cfa->getExit()) {
                isTail = false;
                break;
            }
        }
    }

    return isRecursive && isTail;
}

CallGraph::~CallGraph()
{}

// Visualization
//-----------------------------------------------------------------------------

