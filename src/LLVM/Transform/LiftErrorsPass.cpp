/// \file This file defines the LiftErrorsPass, which lifts error calls
/// from loops and subroutines into the main module.
#include "gazer/LLVM/Transform/Passes.h"
#include "gazer/LLVM/Instrumentation/Check.h"

#include <llvm/Analysis/CallGraph.h>
#include <llvm/Analysis/LoopInfo.h>

using namespace gazer;
using namespace llvm;

namespace
{

class LiftErrorCalls
{
public:
    LiftErrorCalls(llvm::Module& module, llvm::CallGraph& cg, llvm::LoopInfo loops)
        : mModule(module), mCallGraph(cg), mLoopInfo(loops)
    {}

    void run();

private:
    llvm::Module& mModule;
    llvm::CallGraph& mCallGraph;
    llvm::LoopInfo& mLoopInfo;

};

struct LiftErrorCallsPass : public llvm::ModulePass
{
    static char ID;

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override {
        au.addRequired<LoopInfoWrapperPass>();
    }

    bool runOnModule(llvm::Module& module) override;

    llvm::StringRef getPassName() const override { return "Lift error calls into main."; }

};

} // end anonymous namespace

char LiftErrorCallsPass::ID;

void LiftErrorCalls::run()
{
    // Start intraprocedurally: in each function, we want to lift the assertion 
}
