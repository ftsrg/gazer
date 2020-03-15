//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#include "gazer/Automaton/CallGraph.h"
#include "gazer/Automaton/Cfa.h"

#include <llvm/Support/raw_ostream.h>

using namespace gazer;

CallGraph::CallGraph(AutomataSystem& system)
    : mSystem(system)
{
    // Create a call graph node for every procedure
    for (Cfa& cfa : system) {
        mNodes.try_emplace(&cfa, std::make_unique<Node>(&cfa));
    }

    for (Cfa& cfa : system) {
        auto& node = mNodes[&cfa];

        for (Transition* edge : cfa.edges()) {
            if (auto call = llvm::dyn_cast<CallTransition>(edge)) {
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

    for (auto& call : node->mCallsToOthers) {
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

auto CallGraph::lookupNode(Cfa* cfa) -> Node*
{
    return mNodes[cfa].get();
}

CallGraph::~CallGraph()
{}

// Visualization
//-----------------------------------------------------------------------------

