#include "gazer/LLVM/Analysis/ProgramDependence.h"

#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>

using namespace gazer;
using llvm::Instruction;
using llvm::Use;
using llvm::BasicBlock;

std::unique_ptr<ProgramDependenceGraph>
ProgramDependenceGraph::Create(llvm::Function& function, llvm::PostDominatorTree& pdt)
{
    NodeMapTy nodes;
    EdgeContainerTy edges;

    // Create nodes for each instruction.
    for (Instruction& inst : llvm::instructions(function)) {
        nodes[&inst] = std::make_unique<PDGNode>(&inst);
    }

    // Calculate data flow dependencies.
    for (Instruction& inst : llvm::instructions(function)) {
        // All uses of an instruction 'I' flow depend on 'I'
        for (Use& use : inst.operands()) {
            if (llvm::isa<Instruction>(&use)) {
                Instruction* useInst = llvm::dyn_cast<Instruction>(&use);
                if (useInst != &inst) {
                    auto& from = nodes[useInst];
                    auto& to = nodes[&inst];
                    
                    auto& edge = edges.emplace_back(
                        new PDGEdge(from.get(), to.get(), PDGEdge::DataFlow)
                    );

                    from->addOutgoing(edge.get());
                    to->addIncoming(edge.get());
                }
            }
        }

        if (inst.getOpcode() == Instruction::PHI) {
            // PHI nodes may also depend on their incoming blocks
            llvm::PHINode* phi = llvm::dyn_cast<llvm::PHINode>(&inst);
            for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
                BasicBlock* incoming = phi->getIncomingBlock(i);
                auto& from = nodes[incoming->getTerminator()];
                auto& to = nodes[&inst];
                
                auto& edge = edges.emplace_back(
                    new PDGEdge(from.get(), to.get(), PDGEdge::DataFlow)
                );

                from->addOutgoing(edge.get());
                to->addIncoming(edge.get());
            }
        }
    }

    // Calculate control dependencies
    for (BasicBlock& bb : function) {
        for (size_t succIdx = 0; succIdx < bb.getTerminator()->getNumSuccessors(); ++succIdx) {
            auto succ = bb.getTerminator()->getSuccessor(succIdx);

            // Let S consist of all edges (A, B) in the control flow graph...
            BasicBlock *a = &bb;
            BasicBlock *b = succ;

            // ...such that B is not an ancestor of A in the post-dominator tree
            if (!pdt.dominates(b, a)) {
                auto pdomA = pdt.getNode(a);
                auto pdomB = pdt.getNode(b);

                auto& pdgA = nodes[a->getTerminator()];

                // Given (A, B), the desired effect will be achieved by
                // traversing backwards from B in the post-dominator tree
                // until we reach A’s parent (if it exists), marking all nodes
                // visited before A’s parent as control dependent on A.
                auto parent = pdomB;
                while (parent != pdomA->getIDom()) {
                    if (parent == nullptr) {
                        break;
                    }

                    for (auto& inst : *parent->getBlock()) {
                        auto& pdgNode = nodes[&inst];
                        auto& edge =edges.emplace_back(
                            new PDGEdge(pdgA.get(), pdgNode.get(), PDGEdge::Control)
                        );
                        pdgA->addOutgoing(edge.get());
                        pdgNode->addIncoming(edge.get());
                    }

                    parent = parent->getIDom();
                }
            }
        }
    }

    return std::unique_ptr<ProgramDependenceGraph>(new ProgramDependenceGraph(
        &function, nodes, edges
    ));
}

PDGNode* ProgramDependenceGraph::nodeFor(llvm::Value* value)
{
    auto result = mNodes.find(value);
    assert((result != mNodes.end()) && "Nodes should be present in the nodes map");

    return result->second.get();
}

ProgramDependenceWrapperPass::ProgramDependenceWrapperPass()
    : FunctionPass(ID)
{
    initializeProgramDependenceWrapperPassPass(*llvm::PassRegistry::getPassRegistry());
}

char ProgramDependenceWrapperPass::ID = 0;

void ProgramDependenceWrapperPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<llvm::PostDominatorTreeWrapperPass>();
    au.setPreservesAll();
}

bool ProgramDependenceWrapperPass::runOnFunction(llvm::Function& function)
{
    llvm::PostDominatorTree& pdt = getAnalysis<llvm::PostDominatorTreeWrapperPass>().getPostDomTree();
    mPDG = ProgramDependenceGraph::Create(function, pdt);

    return false;
}

namespace
{

struct ProgramDependencePrinterPass final : public llvm::FunctionPass
{
    static char ID;

    ProgramDependencePrinterPass()
        : FunctionPass(ID)
    {}

    virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const override {
        au.addRequired<ProgramDependenceWrapperPass>();
        au.setPreservesAll();
    }

    virtual bool runOnFunction(llvm::Function& function) override {
        auto& pdg = getAnalysis<ProgramDependenceWrapperPass>().getProgramDependenceGraph();
        pdg.print(llvm::errs());

        return false;
    }
};

char ProgramDependencePrinterPass::ID = 0;

} // end anonymous namespace

void ProgramDependenceGraph::print(llvm::raw_ostream& os)
{
    os << "digraph PDG {\n";
    os << "node [shape=\"box\"];\n";
    for (auto& bb : *mFunction) {
        os << "subgraph cluster_" << static_cast<void*>(&bb) << " {\n";
        for (auto& it : bb) {
            os << "node_" << static_cast<void*>(&it) << " [label=\"";
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
            }
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
            << "node_" << static_cast<void*>(edge->getFrom()->getValue())
            << " -> "
            << "node_" << static_cast<void*>(edge->getTo()->getValue())
            << "[color=\"";
        switch (edge->getDependencyType()) {
            case PDGEdge::DataFlow: os << "green"; break;
            case PDGEdge::Control: os << "blue"; break;
        }
        os << "\"];\n";
    }

    os << "}\n";
}

using namespace llvm;

INITIALIZE_PASS_BEGIN(ProgramDependenceWrapperPass, "pdg", "Calculate program dependence graph", true, true)
INITIALIZE_PASS_DEPENDENCY(PostDominatorTreeWrapperPass)
INITIALIZE_PASS_END(ProgramDependenceWrapperPass, "pdg", "Perform backward slicing", false, false)

namespace gazer
{
    llvm::Pass* createProgramDependenceWrapperPass() {
        return new ProgramDependenceWrapperPass();
    }

    llvm::Pass* createProgramDependencePrinterPass() {
        return new ProgramDependencePrinterPass();
    }
}

