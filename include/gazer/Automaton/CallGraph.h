//==- CallGraph.h - Call graph interface for CFAs ---------------*- C++ -*--==//
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
#ifndef GAZER_AUTOMATON_CALLGRAPH_H
#define GAZER_AUTOMATON_CALLGRAPH_H

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/GraphTraits.h>
#include <llvm/ADT/iterator.h>

#include <vector>

namespace gazer
{

class Cfa;
class AutomataSystem;
class CallTransition;

class CallGraph
{
public:
    class Node;
    using CallSite = std::pair<CallTransition*, Node*>;
    
    class Node
    {
        friend class CallGraph;
    public:
        Node(Cfa* cfa)
            : mCfa(cfa)
        {}

        void addCall(CallTransition* call, Node* node)
        {
            assert(call != nullptr);
            assert(node != nullptr);

            mCallsToOthers.emplace_back(call, node);
            node->mCallsToThis.emplace_back(call);
        }

        Cfa* getCfa() const { return mCfa; }

        using iterator = std::vector<CallSite>::const_iterator;
        iterator begin() const { return mCallsToOthers.begin(); }
        iterator end() const { return mCallsToOthers.end(); }

    private:
        Cfa* mCfa;
        std::vector<CallSite> mCallsToOthers;
        std::vector<CallTransition*> mCallsToThis;
    };
public:
    explicit CallGraph(AutomataSystem& system);

    ~CallGraph();

    /// Returns true if the given procedure is tail-recursive. That is,
    /// if it is recursive and the recursive calls only occur in
    /// the procedure directly before the exit.
    bool isTailRecursive(Cfa* cfa);

    AutomataSystem& getSystem() const { return mSystem; };

    Node* lookupNode(Cfa* cfa);

private:
    AutomataSystem& mSystem;
    llvm::DenseMap<Cfa*, std::unique_ptr<Node>> mNodes;
};

} // end namespace gazer

namespace llvm
{

template<>
struct GraphTraits<gazer::CallGraph::Node*>
{
    using NodeRef = gazer::CallGraph::Node*;

    static NodeRef getNodeFromCallSite(const gazer::CallGraph::CallSite cs) { return cs.second; }

    using ChildIteratorType = llvm::mapped_iterator<
        gazer::CallGraph::Node::iterator,
        decltype(&getNodeFromCallSite)
    >;

    static ChildIteratorType child_begin(NodeRef N)
    {
        return ChildIteratorType(N->begin(), &getNodeFromCallSite);
    }

    static ChildIteratorType child_end(NodeRef N)
    {
        return ChildIteratorType(N->end(), &getNodeFromCallSite);
    }

    static NodeRef getEntryNode(gazer::CallGraph::Node* node) {
        return node;
    }
};

} // end namespace llvm

#endif