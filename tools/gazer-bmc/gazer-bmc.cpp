#include "gazer/LLVM/Instrumentation/DefaultChecks.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/LLVMFrontend.h"

#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Verifier/BoundedModelChecker.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>

#include <string>
#include <filesystem>

using namespace gazer;
using namespace llvm;

namespace gazer
{
    extern cl::OptionCategory LLVMFrontendCategory;
    extern cl::OptionCategory IrToCfaCategory;
    extern cl::OptionCategory TraceCategory;
}

namespace
{
    cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);

    cl::OptionCategory BmcAlgorithmCategory("Bounded model checker algorithm settings");

    cl::opt<unsigned> MaxBound("bound", cl::desc("Maximum iterations for the bounded model checker."),
        cl::init(1), cl::cat(BmcAlgorithmCategory));
    cl::opt<unsigned> EagerUnroll("eager-unroll", cl::desc("Eager unrolling bound."), cl::init(0),
        cl::cat(BmcAlgorithmCategory));

    cl::opt<bool> DumpCfa("debug-dump-cfa", cl::desc("Dump the generated CFA after each inlining step."),
        cl::cat(BmcAlgorithmCategory));
    cl::opt<bool> DumpFormula("dump-formula", cl::desc("Dump the solver formula to stderr."),
        cl::cat(BmcAlgorithmCategory));
    cl::opt<bool> DumpSolver("dump-solver", cl::desc("Dump the solver instance to stderr."),
        cl::cat(BmcAlgorithmCategory));
    cl::opt<bool> DumpSolverModel("dump-solver-model", cl::desc("Dump the raw model from the solver to stderr."),
        cl::cat(BmcAlgorithmCategory));

    llvm::cl::opt<bool> PrintSolverStats("print-solver-stats",
        llvm::cl::desc("Print solver statistics information."),
        cl::cat(BmcAlgorithmCategory)
    );
}

static BmcSettings initBmcSettingsFromCommandLine();

int main(int argc, char* argv[])
{
    cl::HideUnrelatedOptions({&LLVMFrontendCategory, &IrToCfaCategory, &TraceCategory});
    cl::ParseCommandLineOptions(argc, argv);

    #ifndef NDEBUG
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::PrettyStackTraceProgram(argc, argv);
    llvm::EnableDebugBuffering = true;
    #endif

    GazerContext context;
    llvm::LLVMContext llvmContext;

    auto settings = LLVMFrontendSettings::initFromCommandLine();
    auto frontend = LLVMFrontend::FromInputFile(InputFilename, context, llvmContext, settings);
    if (frontend == nullptr) {
        return 1;
    }

    // TODO: This should be more flexible.
    if (frontend->getModule().getFunction("main") == nullptr) {
        llvm::errs() << "ERROR: No 'main' function found.\n";
        return 1;
    }

    Z3SolverFactory solverFactory;

    auto bmcSettings = initBmcSettingsFromCommandLine();
    bmcSettings.simplifyExpr = settings.isSimplifyExpr();
    bmcSettings.trace = settings.isTraceEnabled();

    frontend->setBackendAlgorithm(new BoundedModelChecker(solverFactory, bmcSettings));
    frontend->registerVerificationPipeline();

    frontend->run();

    llvm::llvm_shutdown();

    return 0;
}

BmcSettings initBmcSettingsFromCommandLine()
{
    BmcSettings settings;
    settings.debugDumpCfa = DumpCfa;
    settings.dumpFormula = DumpFormula;
    settings.dumpSolver = DumpSolver;
    settings.dumpSolverModel = DumpSolverModel;
    settings.printSolverStats = PrintSolverStats;

    settings.maxBound = MaxBound;
    settings.eagerUnroll = EagerUnroll;

    return settings;
}
