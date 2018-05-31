#include "gazer/LLVM/Analysis/BmcPass.h"
#include "gazer/LLVM/Analysis/CfaBuilderPass.h"
#include "gazer/BMC/BMC.h"
#include "gazer/Z3Solver/Z3Solver.h"

#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>

using namespace gazer;

char BmcPass::ID = 0;

void BmcPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<CfaBuilderPass>();
}

bool BmcPass::runOnFunction(llvm::Function& function)
{
    Automaton& cfa = getAnalysis<CfaBuilderPass>().getCFA();

    Z3Solver solver;
    BoundedModelChecker bmc([](Location* location) {
        return location->getName() == "error";
    }, 100, &solver);
    auto status = bmc.check(cfa);

    llvm::errs() << "Checking " << function.getName() << "...\n";
    if (status == BoundedModelChecker::STATUS_UNSAFE) {
        llvm::errs() << "UNSAFE\n";
    } else {
        llvm::errs() << "UNKNOWN\n";
    }

    return false;
}
