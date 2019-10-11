#include "ThetaVerifier.h"
#include "ThetaCfaGenerator.h"

#include "gazer/Automaton/Cfa.h"

#include <llvm/ADT/Twine.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>

using namespace gazer;
using namespace gazer::theta;

namespace
{

class ThetaVerifierImpl
{
public:
    explicit ThetaVerifierImpl(AutomataSystem& system, ThetaSettings settings, CfaTraceBuilder& traceBuilder)
        : mSystem(system), mSettings(settings), mTraceBuilder(traceBuilder)
    {}

    void writeSystem(llvm::raw_ostream& os);

    /// Runs the theta model checker on the input file.
    std::unique_ptr<VerificationResult> execute(llvm::StringRef input);

private:
    AutomataSystem& mSystem;
    ThetaSettings mSettings;
    CfaTraceBuilder& mTraceBuilder;
};

} // end anonymous namespace

auto ThetaVerifierImpl::execute(llvm::StringRef input) -> std::unique_ptr<VerificationResult>
{
    // First, find Java.
    auto java = llvm::sys::findProgramByName("java");
    if (std::error_code javaEc = java.getError()) {
        return VerificationResult::CreateInternalError("Could not find java. " + javaEc.message());
    }

    // Create the temp file which we will use to dump theta outputs into.
    llvm::SmallString<128> outputsFile;
    auto outputsFileEc = llvm::sys::fs::createTemporaryFile("gazer_theta", "", outputsFile);
    
    if (outputsFileEc) {
        llvm::errs() << outputsFileEc.message() << "\n";
        return VerificationResult::CreateInternalError("Could not create temporary file. " + outputsFileEc.message());
    }

    // Make sure that we have the theta jar and the Z3 library.
    std::string thetaPath = "theta/theta-cfa-cli.jar";
    std::string z3Path = "theta/lib";

    llvm::StringRef args[] = {
        "java",
        "-jar",
        thetaPath,
        "--model", input,
        "--domain",  mSettings.domain,
        "--encoding", mSettings.encoding,
        "--initprec", mSettings.initPrec,
        "--precgranularity", mSettings.precGranularity,
        "--predsplit", mSettings.predSplit,
        "--refinement", mSettings.refinement,
        "--search", mSettings.search,
        "--maxenum", mSettings.maxEnum,
        "--cex",
        "--loglevel", "RESULT"
    };

    std::string ldLibPathEnv = "LD_LIBRARY_PATH=$LD_LIBRARY_PATH:" + z3Path;
    llvm::ArrayRef<llvm::StringRef> env = {
        llvm::StringRef(ldLibPathEnv)
    };

    llvm::Optional<llvm::StringRef> redirects[] = {
        llvm::None,         // stdin
        outputsFile.str(),  // stdout
        llvm::None          // stderr
    };

    std::string thetaErrors;
    int returnCode = llvm::sys::ExecuteAndWait(
        *java,
        args,
        env,
        redirects,
        mSettings.timeout,
        /*memoryLimit=*/0,
        &thetaErrors
    );

    if (returnCode == -1) {
        return VerificationResult::CreateInternalError("Theta execution failed. " + thetaErrors);
    }

    if (returnCode == -2) {
        return VerificationResult::CreateTimeout();
    }

    // Grab the output file's contents.
    auto buffer = llvm::MemoryBuffer::getFile(outputsFile);
    if (auto errorCode = buffer.getError()) {
        return VerificationResult::CreateInternalError("Theta execution failed. " + errorCode.message());
    }

    llvm::StringRef thetaOutput = (*buffer)->getBuffer();

    llvm::outs() << thetaOutput;

    // We have the output from theta, it is now time to parse
    // the result and the possible counterexample.
    if (thetaOutput.startswith("(SafetyResult Safe)")) {
        return VerificationResult::CreateSuccess();
    }

    return VerificationResult::CreateUnknown();
}

void ThetaVerifierImpl::writeSystem(llvm::raw_ostream& os)
{
    theta::ThetaCfaGenerator generator{mSystem};
    generator.write(os);
}

auto ThetaVerifier::check(AutomataSystem& system, CfaTraceBuilder& traceBuilder) -> std::unique_ptr<VerificationResult>
{
    auto settings = ThetaSettings::initFromCommandLine();

    std::error_code errors;
    ThetaVerifierImpl impl(system, settings, traceBuilder);

    // Create a temporary file to write into.
    llvm::SmallString<128> outputFile;
    errors = llvm::sys::fs::createTemporaryFile("gazer_theta_cfa", "theta", outputFile);

    if (errors) {
        return VerificationResult::CreateInternalError(
            "Failed to execute theta verifier. Could not create temporary file. " + errors.message()
        );
    }

    llvm::outs() << "Writing theta CFA to " << outputFile << "\n";
    llvm::raw_fd_ostream thetaOutput(outputFile, errors);

    if (errors) {
        return VerificationResult::CreateInternalError(
            "Failed to execute theta verifier. Could not open output file. " + errors.message()
        );
    }

    impl.writeSystem(thetaOutput);
    thetaOutput.flush();

    return impl.execute(outputFile);
}

auto ThetaSettings::initFromCommandLine() -> ThetaSettings
{
    ThetaSettings settings;

    settings.timeout = 0;
    settings.domain = "PRED_CART";
    settings.refinement = "SEQ_ITP";
    settings.search = "BFS";
    settings.precGranularity = "GLOBAL";
    settings.predSplit = "WHOLE";
    settings.encoding = "LBE";
    settings.maxEnum = "0";
    settings.initPrec = "EMPTY";

    return settings;
}
