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
#include "gazer/LLVM/ClangFrontend.h"

#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/IR/Module.h>

#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/IRReader/IRReader.h>

using namespace llvm;

namespace gazer
{
    cl::OptionCategory ClangFrontendCategory("Clang compilation settings");
}

namespace
{
    // Add some common clang options
    cl::list<std::string> Includes("I", cl::Prefix,cl::ZeroOrMore,
        cl::desc("Add directory to include search path"),
        cl::value_desc("dir"),
        cl::cat(gazer::ClangFrontendCategory)
    );
    cl::list<std::string> Defines("D", cl::Prefix, cl::ZeroOrMore,
        cl::desc("Define <macro> to <value> (or 1 if <value> omitted)"),
        cl::value_desc("macro[=value]"),
        cl::cat(gazer::ClangFrontendCategory)
    );
    cl::list<std::string> Warnings("W", cl::Prefix, cl::ZeroOrMore, cl::value_desc("warning"),
        cl::desc("Enable the specified warning"),
        cl::cat(gazer::ClangFrontendCategory)
    );
}

static bool executeClang(
    llvm::StringRef clang, llvm::StringRef input,
    llvm::StringRef output, llvm::ArrayRef<llvm::StringRef> flags)
{
    // Build our clang configuration
    std::vector<llvm::StringRef> clangArgs = {
        clang,
        "-g",
        // In the newer (>=5.0) versions of clang, -O0 marks functions
        // with a 'not optimizable' flag, which can break the functionality
        // of gazer. Here we request optimizations with -O1 and turn them off
        // immediately by disabling all LLVM passes.
        "-O1", "-Xclang", "-disable-llvm-passes",
        "-c", "-emit-llvm",
    };

    // Add -I and -D options correctly
    std::vector<std::string> prefixedStrings;
    for (auto& include : Includes) {
        prefixedStrings.push_back("-I" + include);
    }
    for (auto& define : Defines) {
        prefixedStrings.push_back("-D" + define);
    }
    for (auto& warning : Warnings) {
        prefixedStrings.push_back("-W" + warning);
    }

    clangArgs.insert(clangArgs.end(), prefixedStrings.begin(), prefixedStrings.end());

    // Add other custom args
    clangArgs.insert(clangArgs.end(), flags.begin(), flags.end());

    clangArgs.insert(clangArgs.end(), {
        input, "-o", output
    });

    std::string clangErrors;

    int returnCode = llvm::sys::ExecuteAndWait(
        clang,
        clangArgs,
        /*env=*/llvm::None,
        /*redirects=*/llvm::None,
        /*secondsToWait=*/0,
        /*memoryLimit=*/0,
        &clangErrors
    );

    if (returnCode == -1) {
        llvm::errs() << "ERROR: failed to execute clang:"
            << (clangErrors.empty() ? "Unknown error." : clangErrors) << "\n";
        return false;
    }

    if (returnCode != 0) {
        llvm::errs() << "ERROR: clang exited with a non-zero exit code.\n";
        return false;
    }

    return true;
}

static bool executeLinker(llvm::StringRef linker, const std::vector<std::string>& bitcodeFiles, llvm::StringRef output)
{
    std::vector<llvm::StringRef> linkerArgs;
    linkerArgs.push_back(linker);
    linkerArgs.insert(linkerArgs.end(), bitcodeFiles.begin(), bitcodeFiles.end());
    linkerArgs.push_back("-o");
    linkerArgs.push_back(output);

    std::string linkerErrors;
    int returnCode = llvm::sys::ExecuteAndWait(
        linker,
        linkerArgs,
        /*env=*/llvm::None,
        /*redirects=*/llvm::None,
        /*secondsToWait=*/0,
        /*memoryLimit=*/0,
        &linkerErrors
    );

    if (returnCode == -1) {
        llvm::errs() << "ERROR: failed to execute llvm-link:"
            << (linkerErrors.empty() ? "Unknown error." : linkerErrors) << "\n";
        return false;
    }

    if (returnCode != 0) {
        llvm::errs() << "ERROR: llvm-link exited with a non-zero exit code.\n";
        return false;
    }

    return true;
}

auto gazer::ClangCompileAndLink(
    llvm::ArrayRef<std::string> files,
    llvm::LLVMContext& llvmContext,
    ClangFrontendSettings& settings)
-> std::unique_ptr<llvm::Module>
{
#define CHECK_ERROR(ERRORCODE, MSG) if (ERRORCODE) {                            \
    llvm::errs() << MSG << "\n";                                                \
    llvm::errs() << (ERRORCODE).message() << "\n";                              \
    return nullptr;                                                             \
}
    std::error_code errorCode;
    llvm::SMDiagnostic err;

    // Find clang and llvm-link.
    auto clang = llvm::sys::findProgramByName("clang");
    CHECK_ERROR(clang.getError(), "Could not find clang.");

    auto llvm_link = llvm::sys::findProgramByName("llvm-link");
    CHECK_ERROR(llvm_link.getError(), "Could not find llvm-link.");

    // Create a temporary working directory
    llvm::SmallString<128> workingDir;
    errorCode = llvm::sys::fs::createUniqueDirectory("gazer_workdir_", workingDir);
    CHECK_ERROR(errorCode, "Could not create temporary working directory.");

    std::vector<std::string> bitcodeFiles;

    for (llvm::StringRef inputFile : files) {
        if (inputFile.endswith_lower(".bc") || inputFile.endswith_lower(".ll")) {
            bitcodeFiles.push_back(inputFile);
            continue;
        }

        if (!inputFile.endswith_lower(".c")) {
            llvm::errs() << "Cannot compile source file " << inputFile << ".\n"
            << "Supported extensions are: .c, .bc, .ll\n";
            return nullptr;
        }

        llvm::SmallString<128> inputPath = inputFile;
        llvm::sys::fs::make_absolute(inputPath);

        // Construct the output file path
        llvm::SmallString<128> outputPath = workingDir;
        llvm::sys::path::append(outputPath, llvm::sys::path::filename(inputPath));
        llvm::sys::path::replace_extension(outputPath, "bc");

        // Add extra flags
        std::vector<llvm::StringRef> flags;
        if (settings.sanitizeOverflow) {
            flags.emplace_back("-ftrapv");
        }

        // Call clang
        bool clangSuccess = executeClang(*clang, inputPath, outputPath, flags);
        if (!clangSuccess) {
            // TODO: Clean-up the working directory?
            llvm::errs() << "Failed to compile input file '" << inputFile << "'.\n";
            return nullptr;
        }

        bitcodeFiles.push_back(outputPath.str());
    }

    // Run llvm-link
    llvm::SmallString<128> resultFile = workingDir;
    llvm::sys::path::append(resultFile, "gazer_llvm_output.bc");
    bool linkerSuccess = executeLinker(*llvm_link, bitcodeFiles, resultFile);

    if (!linkerSuccess) {
        llvm::errs() << "ERROR: failed to execute llvm-link.\n";
        return nullptr;
    }

    // Read back the result file
    auto module = llvm::parseIRFile(resultFile, err, llvmContext);
    if (module == nullptr) {
        err.print(nullptr, llvm::errs());
    }

    return module;
}