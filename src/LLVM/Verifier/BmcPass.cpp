#include "gazer/LLVM/Verifier/BmcPass.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/LLVMTraceBuilder.h"
#include "gazer/LLVM/Instrumentation/Check.h"

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

    LLVMTraceBuilder<Location*> traceBuilder(
        system.getContext(),
        [](Location* loc) -> llvm::BasicBlock* {
            return nullptr;
        },
        getAnalysis<ModuleToAutomataPass>().getVariableMap()
    );

    BoundedModelChecker bmc{solverFactory, &traceBuilder};
    mResult = bmc.check(system);

    if (auto fail = llvm::dyn_cast<FailResult>(mResult.get())) {

        unsigned ec = fail->getErrorID();
        std::string msg = CheckRegistry::GetInstance().messageForCode(ec);
        auto location = fail->getLocation();

        llvm::outs() << "Verification FAILED.\n";
        llvm::outs() << "  " << msg;
        if (location) {
            auto fname = location->getFileName();
            llvm::outs()
                << " in " << (fname != "" ? fname : "<unknown file>")
                << " at line " << location->getLine()
                << " column " << location->getColumn();
        }
        llvm::outs() << ".\n";
    } else if (mResult->isSuccess()) {
        llvm::outs() << "Verification SUCCESSFUL.\n";
    }

    return false;
}
