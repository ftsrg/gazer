#ifndef _GAZER_LLVM_PROGRAMDEPENDENCE_H
#define _GAZER_LLVM_PROGRAMDEPENDENCE_H

#include <llvm/IR/Value.h>
#include <llvm/Analysis/PostDominators.h>

#include <unordered_map>
#include <vector>

namespace llvm {
    void initializeProgramDependenceWrapperPassPass(PassRegistry& registry);
} // end namespace llvm

namespace gazer
{

class PDGNode;

class PDGEdge
{
public:
    enum DependencyType
    {
        DataFlow,
        Control
    };
public:
    PDGEdge(PDGNode* from, PDGNode* to, DependencyType type)
        : mFrom(from), mTo(to), mType(type)
    {}

    PDGNode* getFrom() const { return mFrom; }
    PDGNode* getTo() const { return mTo; }
    DependencyType getDependencyType() const { return mType; }
private:
    PDGNode* mFrom;
    PDGNode* mTo;
    DependencyType mType;
};

class PDGNode
{
    friend class ProgramDependenceGraph;
public:
    PDGNode(llvm::Value* value)
        : mValue(value)
    {}

    llvm::Value* getValue() const { return mValue; }

    using edge_iterator = std::vector<PDGEdge*>::const_iterator;
    edge_iterator incoming_begin()  const { return mIncoming.begin(); }
    edge_iterator incoming_end()    const { return mIncoming.end(); }
    llvm::iterator_range<edge_iterator> incoming() const {
        return llvm::make_range(incoming_begin(), incoming_end());
    }

    edge_iterator outgoing_begin()  const { return mOutgoing.begin(); }
    edge_iterator outgoing_end()    const { return mOutgoing.end(); }
    llvm::iterator_range<edge_iterator> outgoing() const {
        return llvm::make_range(outgoing_begin(), outgoing_end());
    }

private:
    void addIncoming(PDGEdge* edge) { mIncoming.push_back(edge); }
    void addOutgoing(PDGEdge* edge) { mOutgoing.push_back(edge); }

private:
    llvm::Value* mValue;
    std::vector<PDGEdge*> mIncoming;
    std::vector<PDGEdge*> mOutgoing;
};

class ProgramDependenceGraph final
{
    using NodeMapTy = std::unordered_map<llvm::Value*, std::unique_ptr<PDGNode>>;
    using EdgeContainerTy = std::vector<std::unique_ptr<PDGEdge>>;
private:
    ProgramDependenceGraph(
        llvm::Function* function,
        NodeMapTy& nodes,
        EdgeContainerTy& edges
    ) : mFunction(function), mNodes(std::move(nodes)), mEdges(std::move(edges))
    {}

public:
    static std::unique_ptr<ProgramDependenceGraph> Create(llvm::Function& function, llvm::PostDominatorTree& pdt);

    void print(llvm::raw_ostream& os);

    PDGNode* nodeFor(llvm::Value* value);

private:
    llvm::Function* mFunction;
    NodeMapTy mNodes;
    EdgeContainerTy mEdges;
};

class ProgramDependenceWrapperPass final : public llvm::FunctionPass
{
public:
    static char ID;

public:
    ProgramDependenceWrapperPass();

    virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const override;
    virtual bool runOnFunction(llvm::Function& function) override;

    ProgramDependenceGraph& getProgramDependenceGraph() const { return *mPDG; }

private:
    std::unique_ptr<ProgramDependenceGraph> mPDG;    
};

llvm::Pass* createProgramDependenceWrapperPass();

llvm::Pass* createProgramDependencePrinterPass();

}  // end namespace gazer

#endif
