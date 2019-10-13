/// \file A simple tool which dumps a gazer CFA translated from
/// an input LLVM IR file.

#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/Core/GazerContext.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"

#include <llvm/IR/Module.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/PrettyStackTrace.h>

using namespace gazer;
using namespace llvm;

namespace
{
    cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);
    cl::opt<bool> ViewCfa("view", cl::desc("View the CFA in the system's GraphViz viewier."));
    cl::opt<bool> CyclicCfa("cyclic", cl::desc("Represent loops as cycles instead of recursive calls."));
    cl::opt<bool> RunPipeline("run-pipeline", cl::desc("Run the early stages of the verification pipeline, such as instrumentation."));
}

int main(int argc, char* argv[])
{
    cl::ParseCommandLineOptions(argc, argv);
    
    #ifndef NDEBUG
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::PrettyStackTraceProgram(argc, argv);
    llvm::EnableDebugBuffering = true;
    #endif

    GazerContext context;
    llvm::LLVMContext llvmContext;

    auto settings = LLVMFrontendSettings::initFromCommandLine();
    if (CyclicCfa) {
        settings.setLoopRepresentation(LoopRepresentation::Cycle);
    }

    auto frontend = LLVMFrontend::FromInputFile(InputFilename, context, llvmContext, settings);
    if (frontend == nullptr) {
        return 1;
    }

    frontend->registerPass(new gazer::ModuleToAutomataPass(context, settings));
    frontend->registerPass(gazer::createCfaPrinterPass());
    if (ViewCfa) {
        frontend->registerPass(gazer::createCfaViewerPass());
    }

    frontend->run();

    llvm::llvm_shutdown();
}
