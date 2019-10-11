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

    /// Notify the call graph of the removal of an automaton.
    void removeAutomaton(Cfa* cfa);

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

}

#endif