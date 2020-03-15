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
///
/// \file This file defines a base class template for directed graphs.
///
//===----------------------------------------------------------------------===//
#ifndef GAZER_ADT_GRAPH_H
#define GAZER_ADT_GRAPH_H

#include "gazer/ADT/Iterator.h"

#include <llvm/ADT/GraphTraits.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/iterator_range.h>

#include <boost/iterator/indirect_iterator.hpp>

#include <vector>
#include <memory>

namespace gazer
{

template<class NodeTy, class EdgeTy>
class Graph;

template<class NodeTy, class EdgeTy>
class GraphNode
{
    using EdgeVectorTy = std::vector<EdgeTy*>;
    friend class Graph<NodeTy, EdgeTy>;
public:
    // Iterator support
    using edge_iterator = typename EdgeVectorTy::iterator;
    using const_edge_iterator = typename EdgeVectorTy::const_iterator;

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

    const_edge_iterator incoming_begin() const { return mIncoming.begin(); }
    const_edge_iterator incoming_end() const { return mIncoming.end(); }
    llvm::iterator_range<const_edge_iterator> incoming() const {
        return llvm::make_range(incoming_begin(), incoming_end());
    }

    const_edge_iterator outgoing_begin() const { return mOutgoing.begin(); }
    const_edge_iterator outgoing_end() const { return mOutgoing.end(); }
    llvm::iterator_range<const_edge_iterator> outgoing() const {
        return llvm::make_range(outgoing_begin(), outgoing_end());
    }

    size_t getNumIncoming() const { return mIncoming.size(); }
    size_t getNumOutgoing() const { return mOutgoing.size(); }

protected:
    void addIncoming(EdgeTy* edge)
    {
        assert(edge->getTarget() == this);
        mIncoming.emplace_back(edge);
    }

    void addOutgoing(EdgeTy* edge)
    {
        assert(edge->getSource() == this);
        mOutgoing.emplace_back(edge);
    }

    void removeIncoming(EdgeTy* edge)
    {
        mIncoming.erase(std::remove(mIncoming.begin(), mIncoming.end(), edge), mIncoming.end());
    }

    void removeOutgoing(EdgeTy* edge)
    {
        mOutgoing.erase(std::remove(mOutgoing.begin(), mOutgoing.end(), edge), mOutgoing.end());
    }

private:
    EdgeVectorTy mIncoming;
    EdgeVectorTy mOutgoing;
};

template<class NodeTy, class EdgeTy>
class GraphEdge
{
    friend class Graph<NodeTy, EdgeTy>;
public:
    GraphEdge(NodeTy* source, NodeTy* target)
        : mSource(source), mTarget(target)
    {}

    NodeTy* getSource() const { return mSource; }
    NodeTy* getTarget() const { return mTarget; }

private:
    NodeTy* mSource;
    NodeTy* mTarget;
};

/// Represents a graph with a list of nodes and edges.
template<class NodeTy, class EdgeTy>
class Graph
{
    static_assert(std::is_base_of_v<GraphNode<NodeTy, EdgeTy>, NodeTy>, "");
    static_assert(std::is_base_of_v<GraphEdge<NodeTy, EdgeTy>, EdgeTy>, "");

    using NodeVectorTy = std::vector<std::unique_ptr<NodeTy>>;
    using EdgeVectorTy = std::vector<std::unique_ptr<EdgeTy>>;

public:
    using node_iterator = SmartPtrGetIterator<typename NodeVectorTy::iterator>;
    using const_node_iterator = SmartPtrGetIterator<typename NodeVectorTy::const_iterator>;

    node_iterator node_begin() { return mNodes.begin(); }
    node_iterator node_end() { return mNodes.end(); }

    const_node_iterator node_begin() const { return mNodes.begin(); }
    const_node_iterator node_end() const { return mNodes.end(); }

    llvm::iterator_range<node_iterator> nodes() {
        return llvm::make_range(node_begin(), node_end());
    }
    llvm::iterator_range<const_node_iterator> nodes() const {
        return llvm::make_range(node_begin(), node_end());
    }

    using edge_iterator = SmartPtrGetIterator<typename EdgeVectorTy::iterator>;
    using const_edge_iterator = SmartPtrGetIterator<typename EdgeVectorTy::const_iterator>;

    edge_iterator edge_begin() { return mEdges.begin(); }
    edge_iterator edge_end() { return mEdges.end(); }

    const_edge_iterator edge_begin() const { return mEdges.begin(); }
    const_edge_iterator edge_end() const { return mEdges.end(); }
    llvm::iterator_range<edge_iterator> edges() {
        return llvm::make_range(edge_begin(), edge_end());
    }
    llvm::iterator_range<const_edge_iterator> edges() const {
        return llvm::make_range(edge_begin(), edge_end());
    }

    size_t node_size() const { return mNodes.size(); }
    size_t edge_size() const { return mEdges.size(); }

protected:
    void disconnectNode(NodeTy* node)
    {
        for (EdgeTy* edge : node->incoming()) {
            edge->mTarget = nullptr;
            edge->mSource->removeOutgoing(edge);
        }

        for (EdgeTy* edge : node->outgoing()) {
            edge->mSource = nullptr;
            edge->mTarget->removeIncoming(edge);
        }

        node->mIncoming.clear();
        node->mOutgoing.clear();
    }

    void disconnectEdge(EdgeTy* edge)
    {
        edge->mSource->removeOutgoing(edge);
        edge->mTarget->removeIncoming(edge);

        edge->mSource = nullptr;
        edge->mTarget = nullptr;
    }

    void clearDisconnectedElements()
    {
        mNodes.erase(llvm::remove_if(mNodes, [](auto& node) {
            return node->mIncoming.empty() && node->mOutgoing.empty();
        }), mNodes.end());
        mEdges.erase(llvm::remove_if(mEdges, [](auto& edge) {
            return edge->mSource == nullptr || edge->mTarget == nullptr;
        }), mEdges.end());
    }

    template<class... Args>
    NodeTy* createNode(Args&&... args)
    {
        auto& ptr = mNodes.emplace_back(std::make_unique<NodeTy>(std::forward<Args...>(args...)));
        return &*ptr;
    }

    template<class... Args>
    EdgeTy* createEdge(NodeTy* source, NodeTy* target)
    {
        auto& edge = mEdges.emplace_back(std::make_unique<EdgeTy>(source, target));
        
        static_cast<GraphNode<NodeTy, EdgeTy>*>(source)->mOutgoing.emplace_back(&*edge);
        static_cast<GraphNode<NodeTy, EdgeTy>*>(target)->mIncoming.emplace_back(&*edge);

        return &*edge;
    }

    template<class... Args>
    EdgeTy* createEdge(GraphNode<NodeTy, EdgeTy>* source, GraphNode<NodeTy, EdgeTy>* target, Args&&... args)
    {
        auto& edge = mEdges.emplace_back(std::make_unique<EdgeTy>(
            source, target, std::forward<Args...>(args...)
        ));
        
        source->mOutgoing.emplace_back(&*edge);
        target->mIncoming.emplace_back(&*edge);

        return &*edge;
    }

    void addNode(NodeTy* node) { mNodes.push_back(node); }
    void addEdge(EdgeTy* edge) { mEdges.push_back(edge); }

protected:
    NodeVectorTy mNodes;
    EdgeVectorTy mEdges;
};

} // namespace gazer

namespace llvm
{

template<class NodeTy, class EdgeTy>
struct GraphTraits<gazer::Graph<NodeTy, EdgeTy>>
{
    using NodeRef = NodeTy*;
    using EdgeRef = EdgeTy*;

    static constexpr NodeRef GetEdgeTarget(EdgeRef edge) {
        return edge->getTarget();
    }

    static constexpr NodeRef GetEdgeSource(EdgeRef edge) {
        return edge->getSource();
    }

    // Child traversal
    using ChildIteratorType = llvm::mapped_iterator<
        typename NodeTy::edge_iterator, decltype(&GetEdgeTarget), NodeRef>;

    static ChildIteratorType child_begin(NodeRef node) {
        return ChildIteratorType(node->outgoing_begin(), GetEdgeTarget);
    }
    static ChildIteratorType child_end(NodeRef node) {
        return ChildIteratorType(node->outgoing_end(), GetEdgeTarget);
    }

    // Traversing nodes
    using nodes_iterator = typename gazer::Graph<NodeTy, EdgeTy>::const_node_iterator;

    static nodes_iterator nodes_begin(const gazer::Graph<NodeTy, EdgeTy>& graph) {
        return nodes_iterator(graph.node_begin());
    }
    static nodes_iterator nodes_end(const gazer::Graph<NodeTy, EdgeTy>& graph) {
        return nodes_iterator(graph.node_end());
    }

    // Edge traversal
    using ChildEdgeIteratorType = typename NodeTy::edge_iterator;
    static ChildEdgeIteratorType child_edge_begin(NodeRef loc) {
        return loc->outgoing_begin();
    }
    static ChildEdgeIteratorType child_edge_end(NodeRef loc) {
        return loc->outgoing_end();
    }
    static NodeRef edge_dest(EdgeRef edge) {
        return edge->getTarget();
    }

    static unsigned size(const gazer::Graph<NodeTy, EdgeTy>& graph) {
        return graph.node_size();
    }
};

template<class NodeTy, class EdgeTy>
struct GraphTraits<Inverse<gazer::Graph<NodeTy, EdgeTy>>>
    : public GraphTraits<gazer::Graph<NodeTy, EdgeTy>>
{
    using NodeRef = NodeTy*;
    using EdgeRef = EdgeTy*;

    static constexpr NodeRef GetEdgeSource(const EdgeRef edge) {
        return edge->getSource();
    }

    using ChildIteratorType = llvm::mapped_iterator<
        typename NodeTy::edge_iterator, decltype(&GetEdgeSource), NodeRef>;

    static ChildIteratorType child_begin(NodeRef loc) {
        return ChildIteratorType(loc->incoming_begin(), GetEdgeSource);
    }
    static ChildIteratorType child_end(NodeRef loc) {
        return ChildIteratorType(loc->incoming_end(), GetEdgeSource);
    }
};

template<class NodeTy, class EdgeTy>
struct GraphTraits<gazer::GraphNode<NodeTy, EdgeTy>*>
{
    using NodeRef = NodeTy*;

    // Child traversal
    using ChildIteratorType = llvm::mapped_iterator<
        typename NodeTy::edge_iterator, decltype(&GraphTraits<gazer::Graph<NodeTy,EdgeTy>>::GetEdgeTarget), NodeRef>;

    static ChildIteratorType child_begin(NodeRef node) {
        return ChildIteratorType(node->outgoing_begin(), GraphTraits<gazer::Graph<NodeTy,EdgeTy>>::GetEdgeTarget);
    }
    static ChildIteratorType child_end(NodeRef node) {
        return ChildIteratorType(node->outgoing_end(), GraphTraits<gazer::Graph<NodeTy,EdgeTy>>::GetEdgeTarget);
    }

    static NodeRef getEntryNode(NodeTy* node) { return node; }
};

template<class NodeTy, class EdgeTy>
struct GraphTraits<Inverse<gazer::GraphNode<NodeTy, EdgeTy>*>>
{
    using NodeRef = NodeTy*;

    // Child traversal
    using ChildIteratorType = llvm::mapped_iterator<
        typename NodeTy::edge_iterator, decltype(&GraphTraits<gazer::Graph<NodeTy,EdgeTy>>::GetEdgeSource), NodeRef>;

    static ChildIteratorType child_begin(NodeRef node) {
        return ChildIteratorType(node->incoming_begin(), GraphTraits<gazer::Graph<NodeTy,EdgeTy>>::GetEdgeSource);
    }
    static ChildIteratorType child_end(NodeRef node) {
        return ChildIteratorType(node->incoming_end(), GraphTraits<gazer::Graph<NodeTy,EdgeTy>>::GetEdgeSource);
    }

    static NodeRef getEntryNode(Inverse<NodeTy*> node) {
        return node.Graph;
    }
};

} // namespace gazer

#endif
