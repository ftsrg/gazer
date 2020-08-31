#include "lib/ThetaVerifier.h"
#include "lib/ThetaCfaGenerator.h"

#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/Core/GazerContext.h"

#include <llvm/IR/Module.h>
#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

#include <fstream>

#ifndef NDEBUG
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/Debug.h>
#endif

using namespace gazer;
using namespace llvm;

namespace {
    std::map<std::string, std::vector<std::string>> currentAnnotations;
}

/// used by AnnotateSpecialFunctionsPass (rip)
std::vector<std::string> getAnnotationsFor(std::string functionName) {
    auto it = currentAnnotations.find(functionName);
    if (it == currentAnnotations.end()) {
        return std::vector<std::string>();
    }
    return it->second;
}

namespace
{

    // Theta environment settings
    cl::OptionCategory ThetaEnvironmentCategory("Theta environment settings");

    cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<project directory>"),
                              cl::cat(ThetaEnvironmentCategory), cl::init(""));

    cl::opt<std::string> ModelPath("o",
        cl::desc("Model output path (will use a unique location if not set)"),
        cl::cat(ThetaEnvironmentCategory),
        cl::init("")
    );
    cl::opt<bool> ModelOnly("model-only",
        cl::desc("Do not run the verification engine, just write the theta CFA"),
        cl::cat(ThetaEnvironmentCategory)
    );

    cl::opt<std::string> ThetaPath("theta-path",
        cl::desc("Full path to the theta-cfa jar file. Defaults to '<path_to_this_binary>/theta/theta-cfa-cli.jar'"),
        cl::cat(ThetaEnvironmentCategory),
        cl::init("")
    );
    cl::opt<std::string> LibPath("lib-path",
        cl::desc("Full path to the directory containing the Z3 libraries (libz3.so, libz3java.so) required by theta."
                 " Defaults to '<path_to_this_binary>/theta/lib'"),
        cl::cat(ThetaEnvironmentCategory),
        cl::init("")
    );

    // Algorithm options
    cl::OptionCategory ThetaAlgorithmCategory("Theta algorithm settings");

    cl::opt<std::string> Domain("domain", cl::desc("Abstract domain"), cl::init("PRED_CART"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> Refinement("refinement", cl::desc("Refinement strategy"), cl::init("SEQ_ITP"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> Search("search", cl::desc("Search strategy"), cl::init("BFS"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> PrecGranularity("precgranularity", cl::desc("Precision granularity"), cl::init("GLOBAL"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> PredSplit("predsplit", cl::desc("Predicate splitting (for predicate abstraction)"), cl::init("WHOLE"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> Encoding("encoding", cl::desc("Block encoding"), cl::init("LBE"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<int> MaxEnum("maxenum", cl::desc("Maximal number of explicitly enumerated successors"), cl::init(0), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> InitPrec("initPrec", cl::desc("Initial precision of abstraction"), cl::init("EMPTY"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> PruneStrategy("pruneStrategy", cl::desc("Strategy for pruning after refinement"), cl::init("LAZY"), cl::cat(ThetaAlgorithmCategory));
} // end anonymous namespace

namespace gazer
{
    extern cl::OptionCategory ClangFrontendCategory;
    extern cl::OptionCategory LLVMFrontendCategory;
    extern cl::OptionCategory IrToCfaCategory;
    extern cl::OptionCategory TraceCategory;
    extern cl::OptionCategory ChecksCategory;
} // end namespace gazer

static theta::ThetaSettings initSettingsFromCommandLine();

namespace {

/// removes leftover newlines, problem on cross-platform installations
int good_getline(std::istream& ifs, std::string& tmp) {
    if (!std::getline(ifs, tmp)) {
        return 0;
    }
    if (tmp.size() && tmp[tmp.size()-1] == '\r' ) {
        tmp = tmp.substr( 0, tmp.size() - 1 );
    }
    return 1;
}

std::vector<std::string> runnables() {
    const std::string target_path(InputFilename + "/src/");
    const boost::regex my_filter(".*\\.c");

    std::vector<std::string> all_matching_files;

    boost::filesystem::directory_iterator end_itr; // Default ctor yields past-the-end
    for (boost::filesystem::directory_iterator i(target_path); i != end_itr; ++i)
    {
        // Skip if not a file
        if (!boost::filesystem::is_regular_file(i->status())) continue;

        if (i->path().extension() != ".c") continue;

        // File matches, store it

        all_matching_files.push_back(i->path().filename().replace_extension("").string());
    }
    return std::move(all_matching_files);
}

// Runnable -> {Function -> [Annotation]}
using AnnotationMap = std::map<std::string, std::map<std::string, std::vector<std::string>>>;

AnnotationMap annotations() {

    std::ifstream ifs(InputFilename + "/generated/contracts/annotations");
    std::string fileData(std::istreambuf_iterator<char>(ifs),
                     std::istreambuf_iterator<char>());
    std::vector<std::string> lines;
    {
        std::string tmp;
        while (good_getline(ifs, tmp)) {
            lines.push_back(std::move(tmp));
        }
    }
    AnnotationMap result;
    for (const auto& line : lines) {
        if (line == "") {
            continue;
        }
        std::vector<std::string> tokens;
        boost::split(tokens, line, boost::is_any_of(" "));
        if (tokens.size() == 0) {
            continue;
        }
        auto& func = tokens[0];
        auto& re = tokens[1];
        std::vector<std::string> annotations(tokens.begin()+2, tokens.end());
        result.try_emplace(re);
        result[re][func] = annotations;
    }
    return result;
}

using XcfaMap = std::map<std::string, std::vector<std::string>>;

XcfaMap xcfas() {
    // find XCFAs
    std::ifstream ifs(InputFilename + "/generated/contracts/index");
    std::string fileData(std::istreambuf_iterator<char>(ifs),
                     std::istreambuf_iterator<char>());
    std::vector<std::string> lines;
    {
        std::string tmp;
        while (good_getline(ifs, tmp)) {
            lines.push_back(std::move(tmp));
        }
    }
    XcfaMap result;
    for (const auto& line : lines) {
        if (line == "") {
            continue;
        }
        std::vector<std::string> tokens;
        boost::split(tokens, line, boost::is_any_of(" "));
        if (tokens.size() == 0) {
            continue;
        }
        auto& re = tokens[0];
        std::vector<std::string> xcfas(std::next(tokens.begin()), tokens.end());
        result[re] = xcfas;
    }
    return result;
}

}

int main(int argc, char* argv[])
{
    cl::HideUnrelatedOptions({
        &ClangFrontendCategory, &LLVMFrontendCategory,
        &IrToCfaCategory, &TraceCategory, &ChecksCategory,
        &ThetaEnvironmentCategory, &ThetaAlgorithmCategory
    });

    cl::SetVersionPrinter(&FrontendConfigWrapper::PrintVersion);
    cl::ParseCommandLineOptions(argc, argv);

    #ifndef NDEBUG
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::PrettyStackTraceProgram(argc, argv);
    llvm::EnableDebugBuffering = true;
    #endif

    // Set up settings
    theta::ThetaSettings backendSettings = initSettingsFromCommandLine();

    if (backendSettings.thetaCfaPath.empty() || backendSettings.thetaLibPath.empty()) {
        // Find the current program location
        boost::dll::fs::error_code ec;
        auto pathToBinary = boost::dll::program_location(ec);

        if (ec) {
            llvm::errs() << "ERROR: Could not find the path to this process: " + ec.message();
            return 1;
        }

        if (backendSettings.thetaCfaPath.empty()) {
            backendSettings.thetaCfaPath = pathToBinary.parent_path().string() + "/theta/theta-cfa-cli.jar";
        }

        if (backendSettings.thetaLibPath.empty()) {
            backendSettings.thetaLibPath = pathToBinary.parent_path().string() + "/theta/lib";
        }
    }

    auto runnableList = runnables();

    auto annots = annotations();

    auto xcfaData = xcfas();

    // this should be before any LLVM calls (so the destructor is called at an appropriate time)
    llvm::llvm_shutdown_obj mShutdown;

    for (auto& runnable : runnableList) {
        std::string file = InputFilename + "/src/" + runnable + ".c";
        // Find matching annotations
        auto it = annots.find(runnable);
        if (it != annots.end()) {
            currentAnnotations = it->second;
        }

        // Run the frontend

        backendSettings.xcfaDir = InputFilename + "/generated/contracts/";
        backendSettings.xcfas = std::vector<std::string>();
        {
            auto it = xcfaData.find(runnable);
            if (it != xcfaData.end()) {
                backendSettings.xcfas = it->second;
            }
        }
        FrontendConfigWrapper config;

        config.getSettings().function = runnable;

        // Force -math-int
        config.getSettings().ints = IntRepresentation::Integers;
        auto frontend = config.buildFrontend({file});
        if (frontend == nullptr) {
            return 1;
        }

        llvm::outs() << "Running " << runnable << "\n";

        frontend->setBackendAlgorithm(new theta::ThetaVerifier(backendSettings));
        frontend->registerVerificationPipeline();
        frontend->run();
    }
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
    settings.pruneStrategy = PruneStrategy;
    settings.thetaCfaPath = ThetaPath;
    settings.thetaLibPath = LibPath;

    return settings;
}
