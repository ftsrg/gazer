#include "gazer/LLVM/Transform/BoundedUnwindPass.h"

#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Transforms/Utils/UnrollLoop.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/IR/Dominators.h>
#include <llvm/Analysis/AssumptionCache.h>

using namespace gazer;
using namespace llvm;

char BoundedUnwindPass::ID = 0;

void BoundedUnwindPass::getAnalysisUsage(AnalysisUsage& au) const
{
    au.addRequired<LoopInfoWrapperPass>();
    au.addRequired<DominatorTreeWrapperPass>();
    au.addRequired<ScalarEvolutionWrapperPass>();
    au.addRequired<AssumptionCacheTracker>();
}

bool BoundedUnwindPass::runOnFunction(Function& function)
{
    LoopInfo& loopInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    llvm::SmallVector<Loop*, 4> loops(loopInfo.begin(), loopInfo.end());
    for (auto it = loops.begin(); it != loops.end(); ++it) {
        Loop* loop = *it;
        errs() << "Loop unroll in ";
        loop->print(errs(), 0, false);

        auto result = UnrollLoop(
            loop, mBound, mBound,
            true, true, true, false, false,
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
