#include "lib/ThetaVerifier.h"

#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/Core/GazerContext.h"
#include "lib/ThetaCfaGenerator.h"

#include <llvm/IR/Module.h>

#ifndef NDEBUG
#include <llvm/Support/Debug.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <gazer/LLVM/Automaton/ModuleToAutomata.h>

#endif

using namespace gazer;
using namespace llvm;
using namespace llvm;

namespace
{
    cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);
    cl::opt<std::string> ModelPath("o", cl::desc("Model output path (will use a unique location if not set)"), cl::init(""));
    cl::opt<bool> ModelOnly("model-only", cl::desc("Do not run verifier algorithm, just write the theta CFA"));

    cl::OptionCategory ThetaAlgorithmCategory("Theta algorithm settings");

    cl::opt<std::string> Domain("domain", cl::desc("Abstract domain"), cl::init("PRED_CART"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> Refinement("refinement", cl::desc("Refinement strategy"), cl::init("SEQ_ITP"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> Search("search", cl::desc("Search strategy"), cl::init("BFS"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> PrecGranularity("precGranularity", cl::desc("Precision granularity"), cl::init("GLOBAL"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> PredSplit("predSplit", cl::desc("Predicate splitting (for predicate abstraction)"), cl::init("WHOLE"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> Encoding("encoding", cl::desc("Block encoding"), cl::init("LBE"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<int> MaxEnum("maxEnum", cl::desc("Maximal number of explicitly enumerated successors"), cl::init(0), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> InitPrec("initPrec", cl::desc("Initial precision of abstraction"), cl::init("EMPTY"), cl::cat(ThetaAlgorithmCategory));
} // end anonymous namespace

static theta::ThetaSettings initSettingsFromCommandLine();

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

    // Set up settings
    theta::ThetaSettings backendSettings = initSettingsFromCommandLine();

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

    if (!ModelOnly) {
        frontend->setBackendAlgorithm(new theta::ThetaVerifier(backendSettings));
        frontend->registerVerificationPipeline();
        frontend->run();
    } else {
        if (ModelPath.empty()) {
            llvm::errs() << "ERROR: -model-only must be supplied together with -o <path>!\n";
            return 1;
        }

        std::error_code errorCode;
        llvm::raw_fd_ostream rfo(ModelPath, errorCode);

        if (errorCode) {
            llvm::errs() << "ERROR: " << errorCode.message() << "\n";
            return 1;
        }

        // Do not run theta, just generate the model.
        frontend->registerVerificationPipeline();
        frontend->registerPass(theta::createThetaCfaWriterPass(rfo));
        frontend->run();
    }

    llvm::llvm_shutdown();

    return 0;
}

theta::ThetaSettings initSettingsFromCommandLine()
{
    theta::ThetaSettings settings;

    settings.timeout = 0; // TODO
    settings.modelPath = ModelPath;
    settings.domain = Domain;
    settings.refinement = Refinement;
    settings.search = Search;
    settings.precGranularity = PrecGranularity;
    settings.predSplit = PredSplit;
    settings.encoding = Encoding;
    settings.maxEnum = std::to_string(MaxEnum);
    settings.initPrec = InitPrec;

    return settings;
}
