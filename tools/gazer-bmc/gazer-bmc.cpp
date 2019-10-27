#include "gazer/LLVM/Instrumentation/DefaultChecks.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/LLVM/ClangFrontend.h"

#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Verifier/BoundedModelChecker.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

#ifndef NDEBUG
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#endif

#include <string>

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
    cl::list<std::string> InputFilenames(cl::Positional, cl::OneOrMore, cl::desc("<input files>"));

    cl::OptionCategory BmcAlgorithmCategory("Bounded model checker algorithm settings");

    cl::opt<unsigned> MaxBound("bound", cl::desc("Maximum iterations for the bounded model checker."),
        cl::init(100), cl::cat(BmcAlgorithmCategory));
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
    cl::HideUnrelatedOptions({&LLVMFrontendCategory, &BmcAlgorithmCategory, &IrToCfaCategory, &TraceCategory});
    cl::ParseCommandLineOptions(argc, argv);

    #ifndef NDEBUG
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::PrettyStackTraceProgram(argc, argv);
    llvm::EnableDebugBuffering = true;
    #endif

    GazerContext context;
    llvm::LLVMContext llvmContext;

    auto settings = LLVMFrontendSettings::initFromCommandLine();

    // Run the clang frontend
    auto module = ClangCompileAndLink(InputFilenames, llvmContext);
    if (module == nullptr) {
        return 1;
    }

    auto frontend = std::make_unique<LLVMFrontend>(std::move(module), context, settings);

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
