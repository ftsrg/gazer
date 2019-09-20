#include "gazer/LLVM/Verifier/BmcPass.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/LLVMTraceBuilder.h"
#include "gazer/LLVM/Instrumentation/Check.h"
#include "gazer/Trace/TraceWriter.h"
#include "gazer/LLVM/TestGenerator/TestGenerator.h"

#include "gazer/Z3Solver/Z3Solver.h"

#include <llvm/IR/Value.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/FileSystem.h>

namespace gazer
{
    llvm::cl::opt<std::string> TestHarnessFile(
        "test-harness",
        llvm::cl::desc("Write test harness to output file"),
        llvm::cl::value_desc("filename"),
        llvm::cl::init("")
    );

    extern llvm::cl::opt<bool> PrintTrace;
} // end namespace gazer

using namespace gazer;

char BoundedModelCheckerPass::ID;

void BoundedModelCheckerPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<ModuleToAutomataPass>();
    au.setPreservesCFG();
}

bool BoundedModelCheckerPass::runOnModule(llvm::Module& module)
{
    ModuleToAutomataPass& moduleToCfa = getAnalysis<ModuleToAutomataPass>();

    AutomataSystem& system = moduleToCfa.getSystem();
    CfaToLLVMTrace cfaToLlvmTrace = moduleToCfa.getTraceInfo();

    Z3SolverFactory solverFactory;

    LLVMTraceBuilder traceBuilder{system.getContext(), cfaToLlvmTrace};

    BoundedModelChecker bmc{solverFactory, &traceBuilder};
    mResult = bmc.check(system);

    if (auto fail = llvm::dyn_cast<FailResult>(mResult.get())) {

        unsigned ec = fail->getErrorID();
        std::string msg = mChecks.messageForCode(ec);
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

        if (PrintTrace) {
            auto writer = trace::CreateTextWriter(llvm::outs(), true);
            llvm::outs() << "Error trace:\n";
            llvm::outs() << "-----------\n";
            writer->write(fail->getTrace());
        }

        if (TestHarnessFile != "") {
            llvm::outs() << "Generating test harness.\n";
            TestGenerator testGen;
            auto test = testGen.generateModuleFromTrace(
                fail->getTrace(), 
                module.getContext(),
                module
            );

            llvm::StringRef filename(TestHarnessFile);
            std::error_code ec;
            llvm::raw_fd_ostream testOS(filename, ec, llvm::sys::fs::OpenFlags::OF_None);

            if (filename.endswith("ll")) {
                testOS << *test;
            } else {
                llvm::WriteBitcodeToFile(*test, testOS);
            }
        }
    } else if (mResult->isSuccess()) {
        llvm::outs() << "Verification SUCCESSFUL.\n";
    }

    return false;
}
