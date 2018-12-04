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
#include <llvm/Transforms/Utils/ValueMapper.h>

#include <vector>
#include <queue>

#define DEBUG_TYPE "bounded-unwind"

using namespace gazer;
using namespace llvm;

namespace llvm {
    void initializeBoundedUnwindPassPass(PassRegistry& registry);
}

llvm::Pass* gazer::createBoundedUnwindPass(unsigned bound) {
    return new BoundedUnwindPass(bound);
}

using LoopQueueT = llvm::SmallVector<Loop*, 4>;

static bool UnwindLoop(
    unsigned bound,
    Loop* loop,
    BasicBlock* unreachable,
    LoopInfo* loopInfo,
    LoopQueueT& loopsToUnroll)
{
    //llvm::errs() << "Unwind loop: " << *loop << "\n";

    // Get some basic info about the loop
    BasicBlock* preheader = loop->getLoopPreheader();
    BasicBlock* header = loop->getHeader();
    BasicBlock* latch = loop->getLoopLatch();

    if (!preheader) {
        llvm::errs() << "Won't unrool loop: preheader-insertion failed.\n";
        return false;
    }

    if (!latch) {
        llvm::errs() << "Won't unroll loop: loop exit-block-insertion failed.\n";
        return false;
    }

    BasicBlock* lastLatch = latch;

    llvm::SmallDenseMap<BasicBlock*, Value*, 4> newLatches;

    std::vector<BasicBlock*> latches;
    std::vector<BasicBlock*> headers;

    latches.push_back(latch);
    headers.push_back(header);

    // LastVmap holds the values we should use for updating the PHI nodes.
    ValueToValueMapTy lastVmap;
    std::vector<PHINode*> headerPhis;
    for (PHINode& phi : header->phis()) {
        headerPhis.push_back(&phi);
    }

    LoopBlocksDFS dfs(loop);
    dfs.perform(loopInfo);

    auto blockBegin = dfs.beginRPO();
    auto blockEnd = dfs.endRPO();

    for (int cnt = 1; cnt <= bound; ++cnt) {
        //llvm::errs() << "Unrolling cnt " << cnt << "\n";
        llvm::SmallDenseMap<BasicBlock*, BasicBlock*, 4> newBlocks;
        llvm::SmallDenseMap<BasicBlock*, ValueToValueMapTy*, 4> valueMaps;

        llvm::NewLoopsMap newLoops;
        newLoops[loop] = loop;

        for (auto bb = blockBegin; bb != blockEnd; ++bb) {
            ValueToValueMapTy vmap;
            BasicBlock* clonedBB = CloneBasicBlock(*bb, vmap, "." + Twine(cnt));
            header->getParent()->getBasicBlockList().push_back(clonedBB);

            assert((*bb != header || loopInfo->getLoopFor(*bb) == loop)
                && "Header should not be in a sub-loop");

            // If we created a new loop, add it to the unroll queue
            const Loop* oldLoop = addClonedBlockToLoopInfo(*bb, clonedBB, loopInfo, newLoops);
            if (oldLoop) {
                loopsToUnroll.push_back(newLoops[oldLoop]);
            }

            if (*bb == header) {
                headers.push_back(clonedBB);
            }
            if (*bb == latch) {
                latches.push_back(clonedBB);
            }

            newBlocks[*bb] = clonedBB;
            //vmap[*bb] = clonedBB;

            if (*bb == header) {
                for (PHINode* phi : headerPhis) {
                    PHINode* newPhi = cast<PHINode>(vmap[phi]);
                    Value* incoming = newPhi->getIncomingValueForBlock(latch);
                    if (auto inst = dyn_cast<Instruction>(incoming)) {
                        if (cnt > 1 && loop->contains(inst)) {
                            incoming = lastVmap[inst];
                        }
                    }
                    vmap[phi] = incoming;
                    clonedBB->getInstList().erase(newPhi);
                }
            }

            // Update the last values map
            lastVmap[*bb] = clonedBB;
            for (auto vi = vmap.begin(), ve = vmap.end(); vi != ve; ++vi) {
                lastVmap[vi->first] = vi->second;
            }

            // Fix the PHI nodes for the exit blocks
            for (BasicBlock* succ : llvm::successors(*bb)) {
                if (loop->contains(succ)) {
                    continue;
                }

                for (PHINode& phi : succ->phis()) {
                    Value* incoming = phi.getIncomingValueForBlock(*bb);
                    auto result = lastVmap.find(incoming);
                    if (result != lastVmap.end()) {
                        incoming = result->second;
                    }

                    phi.addIncoming(incoming, clonedBB);
                }
            }
        }

        for (auto pair : newBlocks) {
            BasicBlock* clone = pair.second;
            for (auto& inst : *clone) {
                llvm::RemapInstruction(&inst, lastVmap, RF_IgnoreMissingLocals);
            }
        }
    }

    // Fix the preheader PHI nodes
    for (PHINode* phi : headerPhis) {
        phi->replaceAllUsesWith(phi->getIncomingValueForBlock(preheader));
        phi->eraseFromParent();
    }

    // Rewire latch terminators into the headers
    for (unsigned i = 0, e = latches.size(); i < e; ++i) {
        BranchInst* terminator = dyn_cast<BranchInst>(latches[i]->getTerminator());

        unsigned j = i + 1;
        BasicBlock* dst;

        if (i == e - 1) {
            // If this latch is the last one, the destination
            // is the unreachable block
            dst = unreachable;
        } else {
            dst = headers[j];
        }

        if (terminator->isConditional()) {
            unsigned k = 0;
            while (k < terminator->getNumSuccessors()) {
                if (terminator->getSuccessor(k) == headers[i]) {
                    break;
                }
                ++k;
            }

            assert(k != terminator->getNumSuccessors()
                && "Latches should have a branch to the header");

            terminator->setSuccessor(k, dst);
        } else {
            BranchInst::Create(dst, terminator);
            terminator->eraseFromParent();
        }

        // Fix the PHI nodes in the headers after rewiring
        for (PHINode& phi : headers[i]->phis()) {
            int idx = phi.getBasicBlockIndex(latches[i]);
            assert(idx != -1
                && "The header PHI should have an incoming value for a latch");
            
            phi.removeIncomingValue(idx);
        }
    }

    return true;
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
    //errs() << "Unrolling loops for bound " << mBound << ".\n";
    LoopInfo& loopInfo = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    llvm::SmallVector<Loop*, 4> loopsToUnroll(loopInfo.rbegin(), loopInfo.rend());
    

    IRBuilder<> builder(function.getContext());

    //function.viewCFG();

    while (!loopsToUnroll.empty()) {
        Loop* loop = loopsToUnroll.pop_back_val();

        //auto& subLoops = loop->getSubLoopsVector();
        for (Loop* subLoop : loop->getSubLoopsVector()) {
            loopsToUnroll.push_back(subLoop);
        }
        
        BasicBlock* unreachable = BasicBlock::Create(
            function.getContext(),
            loop->getName() + ".not_unrolled",
            &function
        );

        builder.SetInsertPoint(unreachable);
        builder.CreateUnreachable();        

        UnwindLoop(
            mBound,
            loop,
            unreachable,
            &loopInfo,
            loopsToUnroll
        );

        errs() << "\n";
    }

    return true;
}

INITIALIZE_PASS_BEGIN(BoundedUnwindPass, "unwind", "Bounded unwind", false, false)
INITIALIZE_PASS_DEPENDENCY(LoopSimplify)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolutionWrapperPass)
INITIALIZE_PASS_DEPENDENCY(AssumptionCacheTracker)
INITIALIZE_PASS_END(BoundedUnwindPass, "unwind", "Bounded unwind", false, false)

