//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#include "gazer/LLVM/Instrumentation/DefaultChecks.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/LLVM/ClangFrontend.h"

#include "gazer/Z3Solver/Z3Solver.h"
#include "gazer/Verifier/BoundedModelChecker.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/CommandLine.h>

#ifndef NDEBUG
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/Debug.h>
#endif

#include <string>

using namespace gazer;
using namespace llvm;

namespace
{
    cl::list<std::string> InputFilenames(cl::Positional, cl::OneOrMore, cl::desc("<input files>"));

    cl::OptionCategory BmcAlgorithmCategory("Bounded model checker algorithm settings");

    cl::opt<unsigned> MaxBound("bound", cl::desc("Maximum iterations for the bounded model checker"),
        cl::init(100), cl::cat(BmcAlgorithmCategory));
    cl::opt<unsigned> EagerUnroll("eager-unroll", cl::desc("Eager unrolling bound"), cl::init(0),
        cl::cat(BmcAlgorithmCategory));

    cl::opt<bool> DumpCfa("debug-dump-cfa", cl::desc("Dump the generated CFA after each inlining step"),
        cl::cat(BmcAlgorithmCategory));
    cl::opt<bool> DumpFormula("dump-formula", cl::desc("Dump the solver formula to stderr"),
        cl::cat(BmcAlgorithmCategory));
    cl::opt<bool> DumpSolver("dump-solver", cl::desc("Dump the solver instance to stderr"),
        cl::cat(BmcAlgorithmCategory));
    cl::opt<bool> DumpSolverModel("dump-solver-model", cl::desc("Dump the raw model from the solver to stderr"),
        cl::cat(BmcAlgorithmCategory));

    llvm::cl::opt<bool> PrintSolverStats("print-solver-stats",
        llvm::cl::desc("Print solver statistics information"),
        cl::cat(BmcAlgorithmCategory)
    );
}

namespace gazer
{
    extern cl::OptionCategory ClangFrontendCategory;
    extern cl::OptionCategory LLVMFrontendCategory;
    extern cl::OptionCategory IrToCfaCategory;
    extern cl::OptionCategory TraceCategory;
    extern cl::OptionCategory ChecksCategory;
} // end namespace gazer

static BmcSettings initBmcSettingsFromCommandLine();

int main(int argc, char* argv[])
{
    cl::HideUnrelatedOptions({
        &ClangFrontendCategory, &LLVMFrontendCategory, &IrToCfaCategory,
        &TraceCategory, &ChecksCategory, &BmcAlgorithmCategory
    });

    cl::SetVersionPrinter(&FrontendConfigWrapper::PrintVersion);
    cl::ParseCommandLineOptions(argc, argv);

    #ifndef NDEBUG
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::PrettyStackTraceProgram(argc, argv);
    llvm::EnableDebugBuffering = true;
    #endif

    // Create the frontend object
    FrontendConfigWrapper config;
    auto frontend = config.buildFrontend(InputFilenames);
    if (frontend == nullptr) {
        return 1;
    }

    Z3SolverFactory solverFactory;

    auto bmcSettings = initBmcSettingsFromCommandLine();
    bmcSettings.simplifyExpr = frontend->getSettings().simplifyExpr;
    bmcSettings.trace = frontend->getSettings().trace;

    frontend->setBackendAlgorithm(new BoundedModelChecker(solverFactory, bmcSettings));
    frontend->registerVerificationPipeline();

    frontend->run();

    return 0;
}

static BmcSettings initBmcSettingsFromCommandLine()
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
