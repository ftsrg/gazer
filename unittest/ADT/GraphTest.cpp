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
#include "gazer/ADT/Graph.h"

#include <gtest/gtest.h>

using namespace gazer;

namespace
{

struct TestNode;
struct TestEdge;
struct TestGraph;

struct TestNode : public GraphNode<TestNode, TestEdge>
{
    TestNode(std::string name)
        : name(name)
    {}

    std::string name;
};

struct TestEdge : public GraphEdge<TestNode, TestEdge>
{
    using GraphEdge::GraphEdge;
};

struct TestGraph : public Graph<TestNode, TestEdge>
{
    using Graph::createNode;
    using Graph::createEdge;
    using Graph::disconnectEdge;

    void disconnectNode(std::string name) {
        auto it = llvm::find_if(nodes(), [&name] (TestNode* node) { return node->name == name; });
        if (it != node_end()) {
            Graph::disconnectNode(*it);
        }
    }

    using Graph::clearDisconnectedElements;
};

auto example_graph1()
    -> std::unique_ptr<TestGraph>
{
    auto graph = std::make_unique<TestGraph>();

    //            +--> D
    //            |
    // A +--> B --+
    //   |        |
    //   |        +--> D
    //   +--> E

    auto a = graph->createNode("A");
    auto b = graph->createNode("B");
    auto c = graph->createNode("C");
    auto d = graph->createNode("D");
    auto e = graph->createNode("E");

    graph->createEdge(a, b);
    graph->createEdge(b, c);
    graph->createEdge(b, d);
    graph->createEdge(a, e);

    return graph;
}

::testing::AssertionResult sameGraph(
    TestGraph& graph,
    const std::vector<std::string>& expectedNodes,
    const std::vector<std::pair<std::string, std::string>>& expectedEdges)
{
    auto nodeNames = llvm::map_range(graph.nodes(), [](TestNode* n) -> std::string { return n->name; });
    auto edgeNames = llvm::map_range(graph.edges(), [](TestEdge* e) {
        return std::make_pair(e->getSource()->name, e->getTarget()->name);
    });

    if (!std::is_permutation(nodeNames.begin(), nodeNames.end(), expectedNodes.begin(), expectedNodes.end())) {
        std::stringstream ss;
        ss << "Node sets do not match!";
        std::copy(nodeNames.begin(), nodeNames.end(), std::ostream_iterator<std::string>(ss, " "));

        return ::testing::AssertionFailure() << ss.str();
    }

    if (!std::is_permutation(edgeNames.begin(), edgeNames.end(), expectedEdges.begin(), expectedEdges.end())) {
        std::stringstream ss;
        ss << "Edge sets do not match! ";
        for (const std::pair<std::string, std::string>& pair : edgeNames) {
            ss << pair.first << " --> " << pair.second << " ";
        }

        return ::testing::AssertionFailure() << ss.str();
    }

    return ::testing::AssertionSuccess();
}

TEST(GraphTest, testCreate)
{
    auto graph = example_graph1();

    ASSERT_TRUE(sameGraph(
        *graph, { "A", "B", "C", "D", "E" }, { {"A", "B"}, {"B", "C"}, {"B", "D"}, { "A", "E" } }
    ));
}

TEST(GraphTest, testDisconnect)
{
    auto graph = example_graph1();

    graph->disconnectNode("D");
    graph->clearDisconnectedElements();

    ASSERT_TRUE(sameGraph(*graph, { "A", "B", "C", "E" }, { {"A", "B"}, {"B", "C"}, {"A", "E"} }));

    graph->disconnectNode("B");
    graph->clearDisconnectedElements();
    ASSERT_TRUE(sameGraph(*graph, { "A", "E" }, { {"A", "E" }}));
}

}