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


static void UnwindLoop(int bound, Loop* loop, DominatorTree& dt)
{
    BasicBlock* preheader = loop->getLoopPreheader();
    BasicBlock* latch = loop->getLoopLatch();
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

