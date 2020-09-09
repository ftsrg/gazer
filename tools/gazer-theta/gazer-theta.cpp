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
#include "lib/ThetaVerifier.h"
#include "lib/ThetaCfaGenerator.h"

#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/Support/Runtime.h"
#include "gazer/Support/Warnings.h"

#include <llvm/IR/Module.h>
#include <llvm/Support/Path.h>

#ifndef NDEBUG
#include <llvm/Support/Debug.h>
#include <llvm/Support/ErrorOr.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#endif

using namespace gazer;
using namespace llvm;

namespace
{
    cl::list<std::string> InputFilenames(cl::Positional, cl::OneOrMore, cl::desc("<input files>"));

    // Theta environment settings
    cl::OptionCategory ThetaEnvironmentCategory("Theta environment settings");

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
    cl::opt<bool> StackTrace("stacktrace",
        cl::desc("Get full stack trace from Theta in case of an exception"),
        cl::cat(ThetaEnvironmentCategory)
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
    cl::opt<std::string> InitPrec("initprec", cl::desc("Initial precision of abstraction"), cl::init("EMPTY"), cl::cat(ThetaAlgorithmCategory));
    cl::opt<std::string> PruneStrategy("prunestrategy", cl::desc("Strategy for pruning after refinement"), cl::init("LAZY"), cl::cat(ThetaAlgorithmCategory));
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

static bool lookupTheta(llvm::StringRef argvZero, theta::ThetaSettings* settings);

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
    FrontendConfigWrapper config;
    theta::ThetaSettings backendSettings = initSettingsFromCommandLine();
    if (!lookupTheta(argv[0], &backendSettings)) {
        return 1;
    }

    // Force -math-int
    // TODO: Find a way to make this behavior default
    // config.getSettings().ints = IntRepresentation::Integers;

    // Create the frontend object
    auto frontend = config.buildFrontend(InputFilenames);
    if (frontend == nullptr) {
        return 1;
    }

    if (!ModelOnly) {
        frontend->setBackendAlgorithm(new theta::ThetaVerifier(backendSettings));
        frontend->registerVerificationPipeline();
        frontend->run();
    } else {
        if (ModelPath.empty()) {
            emit_error("-model-only must be supplied together with -o <path>!");
            return 1;
        }

        std::error_code errorCode;
        llvm::raw_fd_ostream rfo(ModelPath, errorCode);

        if (errorCode) {
            emit_error("%s\n", errorCode.message().c_str());
            return 1;
        }

        // Do not run theta, just generate the model.
        frontend->registerVerificationPipeline();
        frontend->registerPass(theta::createThetaCfaWriterPass(rfo));
        frontend->run();
    }

    return 0;
}

bool lookupTheta(llvm::StringRef argvZero, theta::ThetaSettings* settings)
{
    if (!settings->thetaCfaPath.empty() && !settings->thetaLibPath.empty()) {
        // All paths are set manually.
        return true;
    }

    // See if we have some environment variables set.
    auto thetaJarEnv = std::getenv("THETA_JAR");
    if (thetaJarEnv != nullptr && settings->thetaCfaPath.empty()) {
        settings->thetaCfaPath = thetaJarEnv;
    }

    auto thetaLibEnv = std::getenv("THETA_LIBS");
    if (thetaLibEnv != nullptr && settings->thetaLibPath.empty()) {
        settings->thetaLibPath = thetaLibEnv;
    }

    // Check if we have everything we need after using the environment variables.
    if (!settings->thetaCfaPath.empty() && !settings->thetaLibPath.empty()) {
        return true;
    }

    // Find the current program location
    llvm::ErrorOr<std::string> pathToBinary = findProgramLocation(argvZero);
    if (auto ec = pathToBinary.getError()) {
        emit_error("Could not find the path to this process: %s\n", ec.message().c_str());
        return false;
    }

    std::string parentPath = llvm::sys::path::parent_path(pathToBinary.get());

    if (settings->thetaCfaPath.empty()) {
        settings->thetaCfaPath = parentPath + "/theta/theta-cfa-cli.jar";
    }

    if (settings->thetaLibPath.empty()) {
        settings->thetaLibPath = parentPath + "/theta/lib";
    }

    return true;
}

theta::ThetaSettings initSettingsFromCommandLine()
{
    theta::ThetaSettings settings;

    settings.timeout = 0; // TODO
    settings.modelPath = ModelPath;
    settings.stackTrace = StackTrace;
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
