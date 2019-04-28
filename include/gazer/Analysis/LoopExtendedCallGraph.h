/// \file This file declares the loop extended call graph pass,
/// which considers loops as call graph nodes as well.
#ifndef GAZER_LOOPEXTENDEDCALLGRAPH_H
#define GAZER_LOOPEXTENDEDCALLGRAPH_H

#include <llvm/Analysis/LoopInfo.h>

namespace gazer
{

class LoopCallGraphNode
{
public:
    enum Kind
    {
        Function, Loop
    };

    explicit LoopCallGraphNode(llvm::Function* function)
        : mFunction(function), mKind(Function)
    {}

    explicit LoopCallGraphNode(llvm::Loop* loop)
        : mLoop(loop), mKind(Loop)
    {}

private:
    union
    {
        llvm::Function* mFunction;
        llvm::Loop* mLoop;
    };
    Kind mKind;
};


class LoopCallGraph
{
    using FunctionMapTy = std::unordered_map<const llvm::Function*, LoopCallGraphNode*>;
    using LoopMapTy = std::unordered_map<const llvm::Loop*, LoopCallGraphNode*>;
public:

private:
    llvm::Module& mModule;

    FunctionMapTy mFunctionMap;
    LoopMapTy mLoopMap;
    std::vector<std::unique_ptr<LoopCallGraphNode>> mNodes;
};

}

#endif //GAZER_LOOPEXTENDEDCALLGRAPH_H
