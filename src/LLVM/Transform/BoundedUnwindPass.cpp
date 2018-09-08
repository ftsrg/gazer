#include "gazer/LLVM/Transform/BoundedUnwindPass.h"

#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Transforms/Utils/UnrollLoop.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/AssumptionCache.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/LoopSimplify.h>
#include <llvm/Transforms/Utils/LoopUtils.h>
#include <llvm/Analysis/LoopPass.h>
#include <llvm/Analysis/LoopIterator.h>

#include <vector>

#define DEBUG_TYPE "bounded-unwind"

using namespace gazer;
using namespace llvm;

namespace llvm {
    void initializeRemoveInfiniteLoopsPassPass(PassRegistry& registry);
    void initializeBoundedUnwindPassPass(PassRegistry& registry);
}

namespace
{

struct RemoveInfiniteLoopsPass final : public FunctionPass
{
    static char ID;

    RemoveInfiniteLoopsPass()
        : FunctionPass(ID)
    {
        initializeRemoveInfiniteLoopsPassPass(*llvm::PassRegistry::getPassRegistry());
    }

    virtual void getAnalysisUsage(AnalysisUsage& au) const
    {
        au.addRequired<LoopInfoWrapperPass>();
    }

    virtual bool runOnFunction(Function& function)
    {
        bool changed = false;

        LoopInfo& li = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
        LLVMContext& context = function.getContext();

        for (Loop* loop : li) {
            BasicBlock* latch  = loop->getLoopLatch();
            BranchInst* br = dyn_cast<BranchInst>(latch->getTerminator());

            if (br->isUnconditional()) {
                //LLVM_DEBUG(dbgs()
                //    << "Adding conditional branch to latch for loop "
                //    << *loop << "\n");

                errs() << "Adding conditional branch to latch for loop " << *loop << "\n";
                // We need to turn this unconditional branch into a conditional one
                auto unreachable = 
                    BasicBlock::Create(function.getContext(), Twine(loop->getName(), "unreachable"), &function);
                BasicBlock* target = br->getSuccessor(0);
                br->removeFromParent();

                BranchInst* newBr = BranchInst::Create(
                    target, unreachable,
                    ConstantInt::getTrue(Type::getInt1Ty(context)),
                    latch
                );
                br->replaceAllUsesWith(newBr);

                br->dropAllReferences();
                br->deleteValue();

                changed = true;
            }
        }

        return changed;
    }

};

}

char RemoveInfiniteLoopsPass::ID = 0;

llvm::Pass* gazer::createRemoveInfiniteLoopsPass() {
    return new RemoveInfiniteLoopsPass();
}

llvm::Pass* gazer::createBoundedUnwindPass(unsigned bound) {
    return new BoundedUnwindPass(bound);
}


static void UnwindLoop(unsigned bound, Loop* loop, DominatorTree& dt)
{
    BasicBlock* preheader = loop->getLoopPreheader();
    BasicBlock* header = loop->getHeader();
    BasicBlock* latch = loop->getLoopLatch();

    for (int cnt = 0; cnt != bound; ++cnt) {
        llvm::SmallDenseMap<BasicBlock*, BasicBlock*, 4> newBlocks;
        llvm::SmallDenseMap<BasicBlock*, ValueToValueMapTy*, 4> valueMaps;

        for (auto bb : loop->blocks()) {
            ValueToValueMapTy* vmap = new ValueToValueMapTy();
            BasicBlock* clonedBB = CloneBasicBlock(bb, *vmap, "." + Twine(cnt));
            header->getParent()->getBasicBlockList().push_back(clonedBB);

            newBlocks[bb] = clonedBB;
            valueMaps[bb] = vmap;
        }

        for (auto pair : newBlocks) {
            BasicBlock* orig = pair.first;
            BasicBlock* clone = pair.second;
            
            // Fix the terminators and SSA nodes for successors
            llvm::TerminatorInst* terminator = clone->getTerminator();
            for (size_t i = 0; i < terminator->getNumSuccessors(); ++i) {
                BasicBlock* succ = terminator->getSuccessor(i);

                auto result = newBlocks.find(succ);
                if (result == newBlocks.end()) { // This one is outside the loop
                    continue;
                }

                BasicBlock* newSucc = result->second;
                terminator->setSuccessor(i, newSucc);

                for (auto it = newSucc->begin(); isa<PHINode>(it); ++it) {
                    auto phi = cast<PHINode>(it);
                    // The PHI node still points to the original block
                    int idx = phi->getBasicBlockIndex(orig);
                    assert(idx != -1 &&
                        "The original block should be present in PHINode");
                    phi->setIncomingBlock(idx, clone);
                }
            }
        }
    }
}

char BoundedUnwindPass::ID = 0;

BoundedUnwindPass::BoundedUnwindPass(unsigned bound)
    : FunctionPass(ID), mBound(bound)
{
    initializeBoundedUnwindPassPass(*llvm::PassRegistry::getPassRegistry());
}

void BoundedUnwindPass::getAnalysisUsage(AnalysisUsage& au) const
{
    au.addRequired<LoopInfoWrapperPass>();
    au.addRequired<DominatorTreeWrapperPass>();
    au.addRequired<ScalarEvolutionWrapperPass>();
    au.addRequired<AssumptionCacheTracker>();
}

bool BoundedUnwindPass::runOnFunction(Function& function)
{
    errs() << "Unrolling loops for bound " << mBound << ".\n";
    LoopInfo& loopInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    llvm::SmallVector<Loop*, 4> loops(loopInfo.begin(), loopInfo.end());
    for (auto it = loops.begin(); it != loops.end(); ++it) {
        Loop* loop = *it;
        //errs() << "Loop unroll in ";
        //loop->print(errs(), 0, false);
        //UnwindLoop(loop, mBound, &loopInfo);

        // TODO: This algorithm fails if the loop is not terminated
        // by a conditional branch.
        auto result = UnrollLoop(
            loop, mBound, mBound,
            true, true, true, true, false,
            1, 0, false,
            &loopInfo,
            &getAnalysis<ScalarEvolutionWrapperPass>().getSE(),
            &getAnalysis<DominatorTreeWrapperPass>().getDomTree(),
            &getAnalysis<AssumptionCacheTracker>().getAssumptionCache(function),
            nullptr, false
        );
        switch (result) {
            case LoopUnrollResult::Unmodified:
                errs() << "Unmodified";
                break;
            case LoopUnrollResult::PartiallyUnrolled:
                errs() << "PartiallyUnrolled";
                break;
            case LoopUnrollResult::FullyUnrolled:
                errs() << "FullyUnrolled";
                break;
        }
        errs() << "\n";
    }

    return true;
}

INITIALIZE_PASS_BEGIN(RemoveInfiniteLoopsPass, "remove-inf-loops", "Remove infinite loops", false, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_END(RemoveInfiniteLoopsPass, "remove-inf-loops", "Remove infinite loops", false, false)

INITIALIZE_PASS_BEGIN(BoundedUnwindPass, "unwind", "Bounded unwind", false, false)
INITIALIZE_PASS_DEPENDENCY(LoopSimplify)
INITIALIZE_PASS_DEPENDENCY(RemoveInfiniteLoopsPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolutionWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_END(BoundedUnwindPass, "unwind", "Bounded unwind", false, false)

