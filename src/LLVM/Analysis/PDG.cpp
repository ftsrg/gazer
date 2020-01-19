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
#include "gazer/LLVM/Analysis/PDG.h"

#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/PostDominators.h>
#include <llvm/Support/GraphWriter.h>

using namespace gazer;

auto ProgramDependenceGraph::Create(
    llvm::Function& function, llvm::PostDominatorTree& pdt
)
    -> std::unique_ptr<ProgramDependenceGraph>
{
    std::unordered_map<llvm::Instruction*, std::unique_ptr<PDGNode>> nodes;
    std::vector<std::unique_ptr<PDGEdge>> edges;

    // Create a node for each instruction.
    for (llvm::Instruction& inst : llvm::instructions(function)) {
        nodes.try_emplace(&inst, new PDGNode(&inst));
    }

    // Collect control dependencies.
    std::unordered_map<llvm::BasicBlock*, llvm::DenseSet<llvm::BasicBlock*>> controlDeps;

    // We are using the algorithm by Ferrante et al.
    for (llvm::BasicBlock& bb : function) {
        for (llvm::BasicBlock* succ : llvm::successors(&bb)) {
            // Let S consist of all edges (A, B) in the control flow graph...
            llvm::BasicBlock *a = &bb;
            llvm::BasicBlock *b = succ;

            // ...such that B is not an ancestor of A in the post-dominator tree.
            if (!pdt.dominates(b, a)) {
                auto domA = pdt.getNode(a);
                auto domB = pdt.getNode(b);

                // Given (A, B), the desired effect will be achieved by
                // traversing backwards from B in the post-dominator tree
                // until we reach A’s parent (if it exists), marking all nodes
                // visited before A’s parent as control dependent on A.
                auto parent = domB;
                while (parent != nullptr && parent != domA->getIDom()) {
                    controlDeps[a].insert(parent->getBlock());
                    parent = parent->getIDom();
                }
            }
        }
    }

    // Insert control dependencies.
    // If a block B control depends on a block A, then all of B's instructions
    // will depend on A's terminator in the PDG.
    for (auto& [block, deps] : controlDeps) {
        llvm::Instruction* terminator = block->getTerminator();
        
        auto& source = nodes[terminator];
        for (llvm::BasicBlock* dependentBlock : deps) {
            for (llvm::Instruction& inst : *dependentBlock) {
                auto& target = nodes[&inst];
                auto& edge = edges.emplace_back(new PDGEdge(&*source, &*target, PDGEdge::Control));
                source->addOutgoing(&*edge);
                target->addIncoming(&*edge);
            }
        }
    }

    // Insert data flow dependencies

    // Collect all memory-access instructions. In order to be conservative, we will
    // assume that all memory-access instructions depend on every call, store and load.
    std::vector<llvm::Instruction*> memoryAccesses;
    for (llvm::Instruction& inst : llvm::instructions(function)) {
        if (inst.mayReadFromMemory()) {
            memoryAccesses.push_back(&inst);
        }
    }

    for (llvm::Instruction& inst : llvm::instructions(function)) {
        // All uses of an instruction 'I' flow depend on 'I'
        for (auto& use_it : inst.operands()) {
            if (llvm::isa<llvm::Instruction>(&use_it)) {
                auto use = llvm::dyn_cast<llvm::Instruction>(&use_it);
                if (use != &inst) {
                    auto& source = nodes[use];
                    auto& target = nodes[&inst];
                    auto& edge = edges.emplace_back(new PDGEdge(&*source, &*target, PDGEdge::DataFlow));

                    source->addOutgoing(&*edge);
                    target->addIncoming(&*edge);
                }
            }
        }

        if (auto phi = llvm::dyn_cast<llvm::PHINode>(&inst)) {
            // PHI nodes may also depend on their incoming blocks
            for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
                llvm::BasicBlock* incoming = phi->getIncomingBlock(i);
                auto& source = nodes[incoming->getTerminator()];
                auto& target = nodes[phi];
                auto& edge = edges.emplace_back(new PDGEdge(&*source, &*target, PDGEdge::DataFlow));

                source->addOutgoing(&*edge);
                target->addIncoming(&*edge);
            }
        }

        // TODO: We could be smarter with some alias analysis here.
        if (inst.mayWriteToMemory()) {
            for (llvm::Instruction* memRead : memoryAccesses) {
                auto& source = nodes[&inst];
                auto& target = nodes[memRead];
                auto& edge = edges.emplace_back(new PDGEdge(&*source, &*target, PDGEdge::Memory));

                source->addOutgoing(&*edge);
                target->addIncoming(&*edge);
            }
        }
    }

    return std::unique_ptr<ProgramDependenceGraph>(new ProgramDependenceGraph(
        function, std::move(nodes), std::move(edges)
    ));
}

void ProgramDependenceGraph::view() const
{
    // Create a random file in which we will dump the PDG
    int fd;
    std::string filename = llvm::createGraphFilename("", fd);

    llvm::raw_fd_ostream os(fd, /*shouldClose=*/ true);
    if (fd == -1) {
        llvm::errs() << "error opening file '" << filename << "' for writing!\n";
        return;
    }

    os << "digraph PDG {\n";
    os << "node [shape=\"box\"];\n";
    for (auto& bb : mFunction) {
        os << "subgraph cluster_" << static_cast<void*>(&bb) << " {\n";
        for (auto& it : bb) {
            os << "node_" << static_cast<void*>(&it) << " [label=\"";
            it.print(os, false); /*
            if (it.getName() != "") {
                it.printAsOperand(os, false);
                os << " = ";
            }
            os << it.getOpcodeName();

            if (it.getNumOperands() != 0) {
                os << " ";
                for (auto op = it.op_begin(); op != it.op_end() - 1; ++op) {
                    (*op)->printAsOperand(os, false);
                    os << ", ";
                }
                (*(it.op_end() - 1))->printAsOperand(os, false);
            } */
            os << "\"];\n";
        }
        os << "graph[style=dashed];\n";
        os << "label= \"";
        bb.printAsOperand(os);
        os << "\";";
        os << "color=black;";
        os << "}\n";
    }

    for (auto& edge : mEdges) {
        os
            << "node_" << static_cast<void*>(edge->getSource()->getInstruction())
            << " -> "
            << "node_" << static_cast<void*>(edge->getTarget()->getInstruction())
            << "[color=\"";
        switch (edge->getKind()) {
            case PDGEdge::DataFlow: os << "green"; break;
            case PDGEdge::Control:  os << "blue"; break;
            case PDGEdge::Memory:   os << "red"; break;
        }
        os << "\"];\n";
    }

    os << "}";

    llvm::errs() << " done. \n";

    llvm::DisplayGraph(filename, false, llvm::GraphProgram::DOT);    
}