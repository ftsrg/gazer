#include "gazer/LLVM/Analysis/BmcPass.h"
#include "gazer/LLVM/Analysis/TopologicalSort.h"
#include "gazer/LLVM/BMC/BmcTrace.h"
#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/LLVM/Instrumentation/Check.h"

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

cl::opt<bool> PrintTrace(
    "trace",
    cl::desc("Print counterexample traces to stdout.")
);

cl::opt<std::string> TestHarnessFile(
    "test-harness",
    cl::desc("Write test harness to output file"),
    cl::value_desc("filename"),
    cl::init("")
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

    Z3SolverFactory solverFactory;
    std::unique_ptr<ExprBuilder> builder;
    if (NoFoldingExpr) {
        builder = CreateExprBuilder();
    } else {
        builder = CreateFoldingExprBuilder();
    }

    Stopwatch<> sw;
    sw.start();
    
    BoundedModelChecker bmc(function, topo, builder.get(), solverFactory, llvm::outs());
    auto result = bmc.run();
    sw.stop();
    llvm::outs() << "Elapsed time: ";
    sw.format(llvm::outs(), "s");
    llvm::outs() << "\n";

    if (result.isUnsafe()) {
        unsigned ec = *(result.getErrorType());
        std::string msg = CheckRegistry::GetInstance().messageForCode(ec);

        llvm::outs() << "Verification FAILED: " << msg;
        llvm::outs() << ".\n";

        if (PrintTrace) {
            auto writer = bmc::CreateTextTraceWriter(llvm::outs());
            llvm::outs() << "Error trace:\n";
            llvm::outs() << "-----------\n";
            writer->write(result.getTrace());
        }

        if (TestHarnessFile != "") {
            llvm::outs() << "Generating test harness.\n";
            TestGenerator testGen;
            auto test = testGen.generateModuleFromTrace(
                result.getTrace(), 
                function.getContext(),
                function.getParent()->getDataLayout()
            );

            StringRef filename(TestHarnessFile);
            std::error_code ec;
            llvm::raw_fd_ostream testOS(filename, ec, sys::fs::OpenFlags::F_None);

            if (filename.endswith("ll")) {
                testOS << *test;
            } else {
                llvm::WriteBitcodeToFile(*test, testOS);
            }
        }
    } else if (result.isSafe()) {
        llvm::outs() << "Verification SUCCESSFUL.\n";
    }

    // We modified the program with the predecessors identifications
    return true;
}
