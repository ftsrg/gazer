#ifndef GAZER_AUTOMATON_CALLGRAPH_H
#define GAZER_AUTOMATON_CALLGRAPH_H

#include <llvm/ADT/DenseMap.h>

#include <vector>

namespace gazer
{

class Cfa;
class AutomataSystem;
class CallTransition;

class CallGraph
{
public:
    class Node
    {
        friend class CallGraph;
        using CallSite = std::pair<CallTransition*, Node*>;
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

private:
    llvm::DenseMap<Cfa*, std::unique_ptr<Node>> mNodes;
};

} // end namespace gazer

#endif