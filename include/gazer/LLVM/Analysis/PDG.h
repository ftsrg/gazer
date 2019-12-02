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
#ifndef GAZER_LLVM_ANALYSIS_PDG_H
#define GAZER_LLVM_ANALYSIS_PDG_H

#include <llvm/Analysis/PostDominators.h>
#include <llvm/Pass.h>

#include <memory>
#include <unordered_map>

namespace gazer
{

class PDGNode;

class PDGEdge
{
    friend class ProgramDependenceGraph;
public:
    enum Kind { Control, DataFlow, Memory };

private:
    PDGEdge(PDGNode* source, PDGNode* target, Kind kind)
        : mSource(source), mTarget(target), mKind(kind)
    {}

    PDGEdge(const PDGEdge&) = delete;
    PDGEdge& operator=(const PDGEdge&) = delete;

public:
    PDGNode* getSource() const { return mSource; }
    PDGNode* getTarget() const { return mTarget; }
    Kind getKind() const { return mKind; }

private:
    PDGNode* mSource;
    PDGNode* mTarget;
    Kind mKind;
};

class PDGNode
{
    friend class ProgramDependenceGraph;
private:
    PDGNode(llvm::Instruction* inst)
        : mInst(inst)
    {}

    PDGNode(const PDGNode&) = delete;
    PDGNode& operator=(const PDGNode&) = delete;
public:
    using edge_iterator = std::vector<PDGEdge*>::iterator;
    edge_iterator incoming_begin() { return mIncoming.begin(); }
    edge_iterator incoming_end() { return mIncoming.end(); }
    llvm::iterator_range<edge_iterator> incoming() {
        return llvm::make_range(incoming_begin(), incoming_end());
    }

    edge_iterator outgoing_begin() { return mOutgoing.begin(); }
    edge_iterator outgoing_end() { return mOutgoing.end(); }
    llvm::iterator_range<edge_iterator> outgoing() {
        return llvm::make_range(outgoing_begin(), outgoing_end());
    }

    llvm::Instruction* getInstruction() const { return mInst; }

private:
    void addIncoming(PDGEdge* edge) { mIncoming.push_back(edge); }
    void addOutgoing(PDGEdge* edge) { mOutgoing.push_back(edge); }

private:
    llvm::Instruction* mInst;
    std::vector<PDGEdge*> mIncoming;
    std::vector<PDGEdge*> mOutgoing;
};

class ProgramDependenceGraph final
{
    using NodeMapTy = std::unordered_map<llvm::Instruction*, std::unique_ptr<PDGNode>>;
private:
    ProgramDependenceGraph(
        llvm::Function& function,
        std::unordered_map<llvm::Instruction*, std::unique_ptr<PDGNode>> nodes,
        std::vector<std::unique_ptr<PDGEdge>> edges
    ) : mFunction(function), mNodes(std::move(nodes)), mEdges(std::move(edges))
    {}

private:
    static PDGNode* getNodeFromIterator(NodeMapTy::value_type& pair) {
        return &*pair.second;
    }

public:
    static std::unique_ptr<ProgramDependenceGraph> Create(
        llvm::Function& function,
        llvm::PostDominatorTree& pdt
    );

    PDGNode* getNode(llvm::Instruction* inst) const {
        return &*mNodes.at(inst);
    }

    using node_iterator = llvm::mapped_iterator<NodeMapTy::iterator, decltype(&getNodeFromIterator)>;
    node_iterator node_begin() { return node_iterator(mNodes.begin(), &getNodeFromIterator); }
    node_iterator node_end()   { return node_iterator(mNodes.end(), &getNodeFromIterator); }

    unsigned node_size() const { return mNodes.size(); }

    llvm::Function& getFunction() const { return mFunction; }
    
    void view() const;

private:
    llvm::Function& mFunction;
    NodeMapTy mNodes;
    std::vector<std::unique_ptr<PDGEdge>> mEdges;
};

class ProgramDependenceWrapperPass final : public llvm::FunctionPass
{
public:
    static char ID;
public:
    ProgramDependenceWrapperPass()
        : FunctionPass(ID)
    {}

    virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const override;
    virtual bool runOnFunction(llvm::Function& function) override;
    virtual llvm::StringRef getPassName() const override { return "ProgramDependenceWrapperPass"; }

    ProgramDependenceGraph& getProgramDependenceGraph() const  { return *mResult; }

private:
    std::unique_ptr<ProgramDependenceGraph> mResult;
};

llvm::FunctionPass* createProgramDependenceWrapperPass();

}

// Graph traits specialization for PDGs
//===----------------------------------------------------------------------===//
namespace llvm
{

template<>
struct GraphTraits<gazer::ProgramDependenceGraph>
{
    using NodeRef = gazer::PDGNode*;
    using EdgeRef = gazer::PDGEdge*;

    static gazer::PDGNode* GetEdgeTarget(gazer::PDGEdge* edge) {
        return edge->getTarget();
    }

    using ChildIteratorType = llvm::mapped_iterator<
        gazer::PDGNode::edge_iterator, decltype(&GetEdgeTarget)
    >;
    static ChildIteratorType child_begin(NodeRef node) {
        return ChildIteratorType(node->outgoing_begin(), &GetEdgeTarget);
    }
    static ChildIteratorType child_end(NodeRef node) {
        return ChildIteratorType(node->outgoing_end(), &GetEdgeTarget);
    }

    using ChildEdgeIteratorType = gazer::PDGNode::edge_iterator;
    static ChildEdgeIteratorType child_edge_begin(NodeRef node) {
        return node->outgoing_begin();
    }
    static ChildEdgeIteratorType child_edge_end(NodeRef node) {
        return node->outgoing_end();
    }
    static NodeRef edge_dest(EdgeRef edge) {
        return edge->getTarget();
    }

    using nodes_iterator = gazer::ProgramDependenceGraph::node_iterator;

    static nodes_iterator nodes_begin(gazer::ProgramDependenceGraph& pdg) {
        return pdg.node_begin();
    }
    static nodes_iterator nodes_end(gazer::ProgramDependenceGraph& pdg) {
        return pdg.node_end();
    }

    static unsigned size(const gazer::ProgramDependenceGraph& pdg) {
        return pdg.node_size();
    }
};

}

#endif