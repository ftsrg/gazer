#include "lib/ThetaCfaGenerator.h"

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
    cl::opt<std::string> OutputFilename("o", cl::desc("Output filename"), cl::Required);

    struct ThetaCfaWriterPass : llvm::ModulePass
    {
        static char ID;

        ThetaCfaWriterPass()
            : ModulePass(ID)
        {}

        void getAnalysisUsage(llvm::AnalysisUsage& au) const override
        {
            au.addRequired<ModuleToAutomataPass>();
            au.setPreservesAll();
        }

        bool runOnModule(llvm::Module& module) override
        {
            auto& system = getAnalysis<ModuleToAutomataPass>().getSystem();

            std::error_code ec;
            llvm::raw_fd_ostream output(OutputFilename, ec);

            if (ec) {
                llvm::errs() << ec.message();
                return false;
            }

            theta::ThetaCfaGenerator generator{system};
            generator.write(output);

            return false;
        }
    };
} // end anonymous namespace

char ThetaCfaWriterPass::ID;

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

    frontend->registerVerificationPipeline();
    frontend->registerPass(new ThetaCfaWriterPass());

    frontend->run();

    llvm::llvm_shutdown();

    return 0;
}