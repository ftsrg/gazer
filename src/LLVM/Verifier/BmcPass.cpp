#include "gazer/LLVM/Verifier/BmcPass.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"

#include "gazer/Z3Solver/Z3Solver.h"

#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>

using namespace gazer;

char BoundedModelCheckerPass::ID;

void BoundedModelCheckerPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<ModuleToAutomataPass>();
    au.setPreservesCFG();
}

bool BoundedModelCheckerPass::runOnModule(llvm::Module& module)
{
    AutomataSystem& system = getAnalysis<ModuleToAutomataPass>().getSystem();
    Z3SolverFactory solverFactory;

    BoundedModelChecker bmc{solverFactory};
    mResult = bmc.check(system);

    if (auto fail = llvm::dyn_cast<FailResult>(mResult.get())) {
        llvm::outs() << "Verification FAILED.\n";
    } else if (mResult->isSuccess()) {
        llvm::outs() << "Verification SUCCESSFUL.\n";
    }

    return false;
}
