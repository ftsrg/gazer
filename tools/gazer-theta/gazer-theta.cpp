#include "lib/ThetaCfaGenerator.h"
#include "lib/ThetaVerifier.h"

#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/Core/GazerContext.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"


#include <llvm/IR/Module.h>

#ifndef NDEBUG
#include <llvm/Support/Debug.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#endif

using namespace gazer;
using namespace llvm;

namespace
{
    cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);
    //cl::opt<std::string> OutputFilename("o", cl::desc("Output filename"), cl::Required);
} // end anonymous namespace

int main(int argc, char* argv[])
{
    cl::ParseCommandLineOptions(argc, argv);

    #ifndef NDEBUG
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::PrettyStackTraceProgram(argc, argv);
    llvm::EnableDebugBuffering = true;
    #endif

    // Set up the basics
    GazerContext context;
    llvm::LLVMContext llvmContext;

    // Force -math-int
    auto settings = LLVMFrontendSettings::initFromCommandLine();
    settings.setIntRepresentation(IntRepresentation::Integers);

    auto frontend = LLVMFrontend::FromInputFile(InputFilename, context, llvmContext, settings);
    if (frontend == nullptr) {
        return 1;
    }

    // TODO: This should be more flexible.
    if (frontend->getModule().getFunction("main") == nullptr) {
        llvm::errs() << "ERROR: No 'main' function found.\n";
        return 1;
    }

    frontend->setBackendAlgorithm(new theta::ThetaVerifier());
    frontend->registerVerificationPipeline();

    frontend->run();

    llvm::llvm_shutdown();

    return 0;
}