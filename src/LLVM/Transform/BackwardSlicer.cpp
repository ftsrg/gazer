#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Analysis/ProgramDependence.h"

#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/PromoteMemToReg.h>
#include <llvm/IR/Constants.h>

#include <deque>
#include <queue>
#include <algorithm>
#include <unordered_map>

using namespace gazer;
using namespace llvm;

namespace llvm {
    void initializeBackwardSlicerPassPass(PassRegistry &registry);
}

namespace
{

// TODO: This should not be hardcoded here
auto SliceOnAsserts = [](llvm::Value* value) -> bool {
    CallInst* call = llvm::dyn_cast<CallInst>(value);
    if (call == nullptr) {
        return false;
    }

    Function* callee = call->getCalledFunction();
    if (callee == nullptr) {
        return false;
    }

    return callee->getName() == "__VERIFIER_error"
        || callee->getName() == "__assert_fail";
};

struct BackwardSlicerPass final : public llvm::ModulePass
{
    static char ID;

    using PredicateTy = std::function<bool(llvm::Instruction*)>;
    PredicateTy mPredicate;

    BackwardSlicerPass(/*PredicateTy predicate*/)
        : ModulePass(ID) , mPredicate(SliceOnAsserts)
    {
        initializeBackwardSlicerPassPass(*PassRegistry::getPassRegistry());
    }

    virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const override;
    virtual bool runOnModule(llvm::Module& module) override;
};

} // end anonymous namespace

void BackwardSlicerPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<ProgramDependenceWrapperPass>();
}

bool BackwardSlicerPass::runOnModule(llvm::Module& module)
{
    std::multimap<Function*, Instruction*> slice_criteria;
    std::set<Function*> sliced_functions;

    for (Function& function : module) {
        if (function.isDeclaration()) {
            continue;
        }

        for (BasicBlock& bb : function) {
            for (Instruction& instr : bb) {
                if (mPredicate(&instr)) {
                    slice_criteria.insert(std::make_pair(&function, &instr));
                    sliced_functions.insert(&function);
                }
            }
        }
    }

    for (auto entry : slice_criteria) {
        auto function = entry.first;
        auto instr = entry.second;

        auto& pdg = getAnalysis<ProgramDependenceWrapperPass>(*function)
            .getProgramDependenceGraph();

        ValueToValueMapTy vmap;
        Function* clone = CloneFunction(function, vmap);

        std::queue<Instruction*> queue;
        queue.push(instr);

        std::set<Instruction*> visited;

        while (!queue.empty()) {
            Instruction* current = queue.front();
            queue.pop();
            visited.insert(current);

            auto pdg_node = pdg.nodeFor(current);
            for (auto edge_it = pdg_node->incoming_begin(); edge_it != pdg_node->incoming_end(); ++edge_it) {
                PDGEdge* edge = *edge_it;
                auto from = edge->getFrom()->getValue();

                if (Instruction* instr = dyn_cast<Instruction>(from)) {
                    if (visited.count(instr) == 0) {
                        queue.push(instr);
                    }
                }
            }
        }

        // We found the relevant dependencies, now we remove the
        // unneeded instructions from the CFG

        // Find unneeded basic blocks
        std::set<BasicBlock*> unneeded_blocks;
        std::set<BasicBlock*> visited_blocks;

        auto unreachable = BasicBlock::Create(clone->getContext(), "_unreachable", clone);
        ReturnInst::Create(clone->getContext(), UndefValue::get(clone->getReturnType()), unreachable);

        for (BasicBlock& bb : *function) {
            bool needed = false;
            for (auto it = bb.begin(); it != bb.end(); ++it) {
                // We found a needed block as at least one instruction inside it was needed
                if (visited.count(&(*it)) != 0) {
                    needed = true;
                } else {
                    // Delete this unneeded instruction if its not a terminator
                    Value* mapped_value = vmap.lookup(&*it);
                    Instruction* instr = dyn_cast<Instruction>(mapped_value);
                    assert(instr && "Instructions should map to instructions in the clone.");

                    if (instr->isTerminator())
                        continue;

                    instr->replaceAllUsesWith(UndefValue::get(instr->getType()));
                    instr->eraseFromParent();
                }
            }

            Value* mapped_value = vmap.lookup(&bb);
            BasicBlock* mapped_bb = dyn_cast<BasicBlock>(mapped_value);
            assert(mapped_bb && "BasicBlocks should map to BasicBlocks in the clone");
            if (!needed) {
                unneeded_blocks.insert(mapped_bb);
            } else {
                visited_blocks.insert(mapped_bb);
            }
        }

        std::list<BasicBlock*> wl;
        std::copy(unneeded_blocks.begin(), unneeded_blocks.end(), std::front_inserter(wl));

        while (!wl.empty()) {
            BasicBlock* block = wl.front();
            wl.pop_front();

            // Find the unneeded regions from this block
            std::queue<BasicBlock*> dfs_queue;
            std::set<BasicBlock*> dfs_visited;
            std::set<BasicBlock*> bottom_blocks;
            std::set<BasicBlock*> top_blocks;

            dfs_queue.push(block);
            while (!dfs_queue.empty()) {
                BasicBlock* current = dfs_queue.front();
                dfs_queue.pop();

                if (dfs_visited.find(current) != dfs_visited.end()) {
                    continue;
                }

                dfs_visited.insert(current);

                for (auto succ_it = succ_begin(current); succ_it != succ_end(current); ++succ_it) {
                    BasicBlock* succ = *succ_it;
                    if (visited_blocks.count(succ) != 0) {
                        // This is a needed child, therefore we mark it as bottom
                        bottom_blocks.insert(succ);
                    } else {
                        // This successor was not needed, continue the DFS
                        dfs_queue.push(succ);
                        wl.remove(succ);
                    }
                }

                for (auto pred_it = pred_begin(current); pred_it != pred_end(current); ++pred_it) {
                    BasicBlock* pred = *pred_it;
                    auto find_res = std::find(visited_blocks.begin(), visited_blocks.end(), pred);
                    if (find_res != visited_blocks.end()) {
                        // This is a needed predecessor, therefore we mark it as top
                        top_blocks.insert(pred);
                    } else {
                        // This successor was not needed, continue the DFS
                        dfs_queue.push(pred);
                        wl.remove(pred);
                    }
                }
            }

            assert(bottom_blocks.size() <= 1 && "There can be only one bottom node for a region");
            BasicBlock* bottom = bottom_blocks.empty() ? nullptr : *(bottom_blocks.begin());

            for (auto bb : top_blocks) {
                Instruction* terminator = bb->getTerminator();
                if (terminator->getOpcode() == Instruction::Br) {
                    BranchInst* br = dyn_cast<BranchInst>(terminator);
                    BranchInst* new_br;
                    if (br->isConditional()) {
                        BasicBlock* then = br->getSuccessor(0);
                        BasicBlock* elze = br->getSuccessor(1);

                        bool then_needed = dfs_visited.count(then) == 0;
                        bool elze_needed = dfs_visited.count(elze) == 0;

                        if (then_needed && !elze_needed) {
                            //updatePhiNodes(bb, elze);
                            new_br = (bottom == nullptr || bottom == then) ?
                                BranchInst::Create(then) :
                                BranchInst::Create(then, bottom, br->getCondition());
                        } else if (!then_needed && elze_needed) {
                            //updatePhiNodes(bb, then);
                            new_br = (bottom == nullptr || bottom == elze) ?
                                BranchInst::Create(elze) :
                                BranchInst::Create(bottom, elze, br->getCondition());
                        } else if (!then_needed && !elze_needed) {
                            //updatePhiNodes(bb, then);
                            //updatePhiNodes(bb, elze);
                            new_br = bottom == nullptr ?
                                BranchInst::Create(unreachable) :
                                BranchInst::Create(bottom);
                        } else {
                            assert(false && "At least the then or else path should be a needed child.");
                        }
                    } else {
                        new_br = bottom == nullptr ?
                            BranchInst::Create(unreachable) :
                            BranchInst::Create(bottom);
                    }

                    ReplaceInstWithInst(br, new_br);
                }
            }
        }

        for (BasicBlock* block : unneeded_blocks) {
            while (PHINode* phi = dyn_cast<PHINode>(block->begin())) {
                phi->replaceAllUsesWith(Constant::getNullValue(phi->getType()));
                block->getInstList().pop_front();
            }

            block->replaceAllUsesWith(unreachable);
        }

        for (BasicBlock* block : unneeded_blocks) {
            block->dropAllReferences();
            block->eraseFromParent();
        }

        BasicBlock* old_entry = &(clone->getEntryBlock());
        BasicBlock* new_entry = BasicBlock::Create(clone->getContext(), "entry", clone, old_entry);
        BranchInst::Create(old_entry, new_entry);
    }

    // Delete the original functions
    for (Function* function : sliced_functions) {
        function->eraseFromParent();
    }

    // If we have no sliced functions, then no changes were made
    return !slice_criteria.empty();
}

char BackwardSlicerPass::ID = 0;

INITIALIZE_PASS_BEGIN(BackwardSlicerPass, "slice", "Perform backward slicing", false, false)
INITIALIZE_PASS_DEPENDENCY(ProgramDependenceWrapperPass)
INITIALIZE_PASS_END(BackwardSlicerPass, "slice", "Perform backward slicing", false, false)

namespace gazer
{

llvm::Pass* createBackwardSlicerPass() {
    return new BackwardSlicerPass(/*SliceOnAsserts*/);
}

}
