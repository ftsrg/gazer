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
#include "ThetaVerifier.h"
#include "ThetaCfaGenerator.h"

#include "gazer/Automaton/Cfa.h"
#include "gazer/Support/SExpr.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/APInt.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/CommandLine.h>

using namespace gazer;
using namespace gazer::theta;

using llvm::cast;
using llvm::dyn_cast;

namespace
{

llvm::cl::opt<bool> PrintRawCex("print-raw-cex", llvm::cl::desc("Print the raw counterexample from theta."));

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
    std::unique_ptr<Trace> parseCex(llvm::StringRef cex, unsigned* errorCode);

private:
    AutomataSystem& mSystem;
    ThetaSettings mSettings;
    CfaTraceBuilder& mTraceBuilder;
    ThetaNameMapping mNameMapping;
};

} // end anonymous namespace

static auto createTemporaryFiles(llvm::SmallVectorImpl<char>& outputs, llvm::SmallVectorImpl<char>& cex)
    -> std::error_code
{
    std::error_code ec = llvm::sys::fs::createTemporaryFile("gazer_theta", "", outputs);
    if (ec) {
        return ec;
    }

    ec = llvm::sys::fs::createTemporaryFile("gazer_theta_cex", "", cex);
    if (ec) {
        return ec;
    }

    return std::error_code();
}

auto ThetaVerifierImpl::execute(llvm::StringRef input) -> std::unique_ptr<VerificationResult>
{
    llvm::outs() << "  Building theta configuration.\n";
    // First, find Java.
    auto java = llvm::sys::findProgramByName("java");
    if (std::error_code javaEc = java.getError()) {
        return VerificationResult::CreateInternalError("Could not find java. " + javaEc.message());
    }

    // Create the temp file which we will use to dump theta outputs into.
    llvm::SmallString<128> outputsFile;
    llvm::SmallString<128> cexFile;

    auto ec = createTemporaryFiles(outputsFile, cexFile);
    if (ec) {
        llvm::errs() << ec.message() << "\n";
        return VerificationResult::CreateInternalError("Could not create temporary file. " + ec.message());
    }

    // Make sure that we have the theta jar and the Z3 library.
    llvm::SmallString<128> thetaPath(mSettings.thetaCfaPath);
    llvm::SmallString<128> z3Path(mSettings.thetaLibPath);

    llvm::sys::fs::make_absolute(thetaPath);
    llvm::sys::fs::make_absolute(z3Path);

    if (!llvm::sys::fs::exists(thetaPath)) {
        return VerificationResult::CreateInternalError(
            "Could not execute theta verifier. Tool was not found in path '" + thetaPath + "'."
        );
    }

    if (!llvm::sys::fs::exists(z3Path)) {
        return VerificationResult::CreateInternalError(
            "Could not execute theta verifier."
            "Libraries required by theta (libz3.so) were not found in path '" + z3Path + "'."
        );
    }

    std::string javaLibPath = ("-Djava.library.path=" + z3Path).str();

    std::vector<llvm::StringRef> args = {
        "java",
        javaLibPath,
        "-Xss8m",
        "-Xmx4G",
        "-jar",
        thetaPath,
        "--model", input,
        "--domain",  mSettings.domain,
        "--encoding", mSettings.encoding,
        "--initprec", mSettings.initPrec,
        "--prunestrategy", mSettings.pruneStrategy,
        "--precgranularity", mSettings.precGranularity,
        "--predsplit", mSettings.predSplit,
        "--refinement", mSettings.refinement,
        "--search", mSettings.search,
        "--maxenum", mSettings.maxEnum,
        "--loglevel", "RESULT",
        "--cex", cexFile
    };

    if (mSettings.stackTrace) {
        args.push_back("--stacktrace");
    }
    std::string ldLibPathEnv = ("LD_LIBRARY_PATH=" + z3Path).str();
    std::vector<llvm::StringRef> env = {
        ldLibPathEnv
    };

    llvm::Optional<llvm::StringRef> redirects[] = {
        llvm::None,         // stdin
        outputsFile.str(),  // stdout
        llvm::None          // stderr
    };

    llvm::outs() << "  Built command: '"
         << llvm::join(env, " ") << " "
         << llvm::join(args, " ") << "'.\n";
    llvm::outs() << "  Running theta...\n";
    std::string thetaErrors;
    int returnCode = llvm::sys::ExecuteAndWait(
        *java,
        args,
        llvm::Optional<llvm::ArrayRef<llvm::StringRef>>(env),
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

    if (returnCode != 0) {
        return VerificationResult::CreateInternalError("Theta returned a non-zero exit code. " + thetaErrors);
    }

    // Grab the output file's contents.
    auto buffer = llvm::MemoryBuffer::getFile(outputsFile);
    if (auto errorCode = buffer.getError()) {
        return VerificationResult::CreateInternalError("Theta execution failed. " + errorCode.message());
    }

    llvm::StringRef thetaOutput = (*buffer)->getBuffer();

    // We have the output from theta, it is now time to parse
    // the result and the possible counterexample.
    if (thetaOutput.startswith("(SafetyResult Safe)")) {
        return VerificationResult::CreateSuccess();
    }

    if (thetaOutput.startswith("(SafetyResult Unsafe")) {
        // Parse the counterexample
        buffer = llvm::MemoryBuffer::getFile(cexFile);
        if (auto errorCode = buffer.getError()) {
            return VerificationResult::CreateInternalError("Could not open theta counterexample file. " + errorCode.message());
        }

        llvm::StringRef cexFileContents = (*buffer)->getBuffer();
        auto cexPos = cexFileContents.find("(Trace");
        if (cexPos == llvm::StringRef::npos) {
            llvm::errs() << "Theta returned no parseable counterexample.\n";
            return VerificationResult::CreateFail(VerificationResult::GeneralFailureCode, nullptr);
        }

        auto cex = cexFileContents.substr(cexPos).trim();

        if (PrintRawCex) {
            llvm::outs() << cex << "\n";
        }

        unsigned failureCode = VerificationResult::GeneralFailureCode;
        auto trace = this->parseCex(cex, &failureCode);

        return VerificationResult::CreateFail(failureCode, std::move(trace));
    }

    return VerificationResult::CreateInternalError("Theta returned unrecognizable output. Raw output is:\n" + thetaOutput);
}

void ThetaVerifierImpl::writeSystem(llvm::raw_ostream& os)
{
    theta::ThetaCfaGenerator generator{mSystem};
    generator.write(os, mNameMapping);
}

static void reportInvalidCex(llvm::StringRef message, llvm::StringRef cex, sexpr::Value* value = nullptr)
{
    llvm::errs() << "Could not parse theta counterexample: " <<  message << "\n";
    if (value != nullptr) {
        llvm::errs() << "Value is: ";
        value->print(llvm::errs());
        llvm::errs() << "\n";
    }
    llvm::errs() << "Raw counterexample is: " << cex << "\n";
}

static auto parseLiteral(sexpr::Value* sexpr, Type& varTy) -> ExprRef<LiteralExpr>;

static auto parseBoolLiteral(sexpr::Value* sexpr, BoolType& varTy) -> ExprRef<LiteralExpr>
{
    llvm::StringRef value = sexpr->asAtom();
    if (value.equals_lower("true")) {
        return BoolLiteralExpr::True(varTy.getContext());
    } else if (value.equals_lower("false")) {
        return BoolLiteralExpr::False(varTy.getContext());
    } else {
        return nullptr;
    }
}

static auto parseIntLiteral(sexpr::Value* sexpr, IntType& varTy) -> ExprRef<LiteralExpr>
{
    llvm::StringRef value = sexpr->asAtom();
    long long int intVal;
    if (!value.getAsInteger(10, intVal)) {
        return IntLiteralExpr::Get(varTy, intVal);
    }
    
    return nullptr;
}

static auto parseBvLiteral(sexpr::Value* sexpr, BvType& varTy) -> ExprRef<LiteralExpr>
{
    llvm::StringRef value = sexpr->asAtom();
    llvm::APInt intVal;

    if (!value.getAsInteger(10, intVal)) {
        // Check if we have decimal format
        return BvLiteralExpr::Get(varTy, intVal.zextOrTrunc(varTy.getWidth()));
    }

    if (const auto lit = value.split("'"); !std::get<1>(lit).empty()) {
        // Check if we have Theta literal format

        llvm::StringRef bvSizeStr = std::get<0>(lit);
        char bvLitForm = std::get<1>(lit)[0];
        llvm::StringRef bvLitValueStr = std::get<1>(lit).substr(1);

        unsigned bvSize;

        if (!bvSizeStr.getAsInteger(10, bvSize) && bvSize == varTy.getWidth()) {
            // Check if we have valid bitvector size

            switch (std::tolower(bvLitForm)) {
                // Check if we have valid bitvector literal type

                case 'b':
                    if (!bvLitValueStr.getAsInteger(2, intVal)) {
                        return BvLiteralExpr::Get(varTy, intVal.zextOrTrunc(varTy.getWidth()));
                    }
                    break;
                case 'x':
                    if (!bvLitValueStr.getAsInteger(16, intVal)) {
                        return BvLiteralExpr::Get(varTy, intVal.zextOrTrunc(varTy.getWidth()));
                    }
                    break;
                case 'd':
                    if (!bvLitValueStr.getAsInteger(10, intVal)) {
                        return BvLiteralExpr::Get(varTy, intVal.zextOrTrunc(varTy.getWidth()));
                    }
                    break;
            }
        }
    }

    return nullptr;
}

static auto parseArrayLiteral(sexpr::Value* sexpr, ArrayType& varTy) -> ExprRef<LiteralExpr>
{
    auto arrLitImpl = sexpr->asList();
    // The string "array" at the beginning
    arrLitImpl.erase(arrLitImpl.begin());

    ArrayLiteralExpr::MappingT entries;
    ExprRef<LiteralExpr> def = nullptr;

    for (auto *arrEntryImpl : arrLitImpl) {
        auto *keyImpl = arrEntryImpl->asList()[0];
        auto *valueImpl = arrEntryImpl->asList()[1];

        if (keyImpl->isAtom() && keyImpl->asAtom() == "default") {
            def = parseLiteral(valueImpl, varTy.getElementType());
        } else {
            entries[parseLiteral(keyImpl, varTy.getIndexType())] = parseLiteral(valueImpl, varTy.getElementType());
        }
    }

    return ArrayLiteralExpr::Get(varTy, entries, def);
}

static auto parseLiteral(sexpr::Value* sexpr, Type& varTy) -> ExprRef<LiteralExpr>
{
    switch (varTy.getTypeID()) {
        case Type::BoolTypeID:
            return parseBoolLiteral(sexpr, cast<BoolType>(varTy));
        case Type::IntTypeID:
            return parseIntLiteral(sexpr, cast<IntType>(varTy));
        case Type::BvTypeID:
            return parseBvLiteral(sexpr, cast<BvType>(varTy));
        case Type::ArrayTypeID:
            return parseArrayLiteral(sexpr, cast<ArrayType>(varTy));
        default:
            return nullptr;
    }
}

std::unique_ptr<Trace> ThetaVerifierImpl::parseCex(llvm::StringRef cex, unsigned* errorCode)
{
    // TODO: The whole parsing process is very fragile. We should introduce some proper error cheks.
    auto trace = sexpr::parse(cex);
    assert(trace->isList());

    std::vector<Location*> states;
    std::vector<std::vector<VariableAssignment>> actions;
    llvm::StringRef initLocName = trace->asList()[1]->asList()[1]->asAtom();

    states.push_back(mNameMapping.locations[initLocName]);

    for (size_t i = 3; i < trace->asList().size(); i += 2) {
        auto& stateList = trace->asList()[i]->asList();
        if (stateList[0]->asAtom() != "CfaState") {
            reportInvalidCex("expected 'CfaState' atom in list", cex, &*(trace->asList()[i]));
            return nullptr;
        }

        // Theta may insert unnamed locations, thus the format is either
        // (CfaState locName (ExplState ...)) or (CfaState (ExplState ...)).
        // In the latter case, we must skip this location as it is not present
        // anywhere within gazer.
        if (stateList[1]->isList()) {
            continue;
        }

        auto& actionList = stateList[2]->asList();

        if (actionList.size() < 2) {
            // `(CfaState (ExplState))`, nothing to do here.
            actions.push_back({});
        } else {
            std::vector<VariableAssignment> assigns;
            for (size_t j = 1; j < actionList.size(); ++j) {
                llvm::StringRef varName = actionList[j]->asList()[0]->asAtom();

                Variable* variable = mNameMapping.variables.lookup(varName);
                assert(variable != nullptr && "Each variable must be present in the theta name mapping!");

                Variable* origVariable = mNameMapping.inlinedVariables.lookup(variable);
                if (origVariable == nullptr) {
                    origVariable = variable;
                }

                auto rhs = parseLiteral(actionList[j]->asList()[1], origVariable->getType());
                if (rhs == nullptr) {
                    reportInvalidCex("expected a valid integer, bitvector, boolean or their array as value", cex, &*(actionList[j]->asList()[1]));
                    return nullptr;
                }

                assigns.push_back({origVariable, rhs});
            }

            actions.push_back(assigns);
        }

        llvm::StringRef locName = stateList[1]->asAtom();

        Location* loc = mNameMapping.locations.lookup(locName);
        Location* origLoc = mNameMapping.inlinedLocations.lookup(loc);

        if (origLoc == nullptr) {
            origLoc = loc;
        }

        states.push_back(origLoc);
    }

    // Find the value of the error field variable and extract the error code.
    auto& lastAction = actions.back();
    auto errAssign = std::find_if(lastAction.begin(), lastAction.end(), [this](VariableAssignment& assignment) {
        return assignment.getVariable() == mNameMapping.errorFieldVariable;
    });

    assert(errAssign != lastAction.end()
        && "The error field variable must be present in the last element of the trace!");

    auto errorExpr = llvm::dyn_cast_or_null<LiteralExpr>(errAssign->getValue());
    assert(errorExpr != nullptr && "The error field must be present in the trace as a literal expression!");

    unsigned ec = 0;
    if (auto bvLit = llvm::dyn_cast<BvLiteralExpr>(errorExpr)) {
        ec = bvLit->getValue().getLimitedValue();
    } else if (auto intLit = llvm::dyn_cast<IntLiteralExpr>(errorExpr)) {
        ec = intLit->getValue();
    } else {
        llvm_unreachable("Invalid error field type!");
    }

    *errorCode = ec;

    return mTraceBuilder.build(states, actions);
}

auto ThetaVerifier::check(AutomataSystem& system, CfaTraceBuilder& traceBuilder)
    -> std::unique_ptr<VerificationResult>
{
    llvm::outs() << "Running theta verification backend.\n";

    std::error_code errors;
    ThetaVerifierImpl impl(system, mSettings, traceBuilder);

    // Create a temporary file to write into.
    llvm::SmallString<128> outputFile;
    errors = llvm::sys::fs::createTemporaryFile("gazer_theta_cfa", "theta", outputFile);

    if (errors) {
        return VerificationResult::CreateInternalError(
            "Failed to execute theta verifier. Could not create temporary file. " + errors.message()
        );
    }

    llvm::outs() << "  Writing theta CFA into '" << outputFile << "'.\n";
    llvm::raw_fd_ostream thetaOutput(outputFile, errors);

    if (errors) {
        return VerificationResult::CreateInternalError(
            "Failed to execute theta verifier. Could not open output file. " + errors.message()
        );
    }

    impl.writeSystem(thetaOutput);

    return impl.execute(outputFile);
}
