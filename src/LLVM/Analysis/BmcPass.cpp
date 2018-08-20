#include "gazer/LLVM/Analysis/BmcPass.h"
#include "gazer/LLVM/Analysis/TopologicalSort.h"
#include "gazer/LLVM/BMC/BmcTrace.h"
#include "gazer/Z3Solver/Z3Solver.h"

#include "gazer/LLVM/BMC/BMC.h"
#include "gazer/LLVM/TestGenerator/TestGenerator.h"

#include "gazer/Support/Stopwatch.h"

#include <llvm/IR/Function.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/CommandLine.h>

using namespace gazer;
using namespace llvm;

namespace
{

cl::opt<bool> NoFoldingExpr(
    "no-folding-expr",
    cl::desc("Do not fold and simplify expressions. Use only for debugging.")
);

}

char BmcPass::ID = 0;

void BmcPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<TopologicalSortPass>();
    au.setPreservesCFG();
}

bool BmcPass::runOnFunction(llvm::Function& function)
{
    TopologicalSort& topo = getAnalysis<TopologicalSortPass>()
        .getTopologicalSort();    

    Stopwatch<> sw;
    Z3SolverFactory solverFactory;
    std::unique_ptr<ExprBuilder> builder;
    if (NoFoldingExpr) {
        builder = CreateExprBuilder();
    } else {
        builder = CreateFoldingExprBuilder();
    }

    sw.start();
    
    BoundedModelChecker bmc(function, topo, builder.get(), solverFactory, llvm::errs());
    auto result = bmc.run();

    sw.stop();
    llvm::errs() << "Elapsed time: ";
    sw.format(llvm::errs(), "s");
    llvm::errs() << "\n";

    if (result.getStatus() == BmcResult::Unsafe) {
        TestGenerator testGen;
        auto test = testGen.generateModuleFromTrace(
            result.getTrace(), 
            function.getContext(),
            function.getParent()->getDataLayout()
        );

        std::error_code ec;
        llvm::raw_fd_ostream testOS("harness.bc", ec, sys::fs::OpenFlags::F_None);
        llvm::WriteBitcodeToFile(*test, testOS);
    }

    // We modified the CFG with the predecessors identifications
    return true;
}
