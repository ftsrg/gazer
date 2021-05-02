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
#include "gazer/LLVM/LLVMFrontend.h"
#include "gazer/LLVM/Instrumentation/DefaultChecks.h"
#include "gazer/Support/Warnings.h"
#include "gazer/Trace/WitnessWriter.h"
#include "gazer/Config/gazer-config.h"

#include <llvm/IR/Module.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/Support/Path.h>

#ifndef NDEBUG
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/Debug.h>
#endif

using namespace gazer;

FrontendConfig::FrontendConfig()
    : mSettings(LLVMFrontendSettings::initFromCommandLine())
{
    // Insert default checks
    registerCheck("assertion-fail",     &checks::createAssertionFailCheck);
    registerCheck("div-by-zero",        &checks::createDivisionByZeroCheck);
    registerCheck("signed-overflow",    &checks::createSignedIntegerOverflowCheck);
}

void FrontendConfig::registerCheck(llvm::StringRef name, CheckFactory factory)
{
    mFactories.emplace(name, factory);
}

auto FrontendConfig::buildFrontend(
    llvm::ArrayRef<std::string> inputs,
    GazerContext& context,
    llvm::LLVMContext& llvmContext
) -> std::unique_ptr<LLVMFrontend>
{
    std::vector<std::unique_ptr<Check>> checks;
    createChecks(checks);

    auto llvmModule = ClangCompileAndLink(inputs, llvmContext, mClangSettings);
    if (llvmModule == nullptr) {
        llvm::errs() << "Failed to build input llvmModule.\n";
        return nullptr;
    }

    if (!mSettings.validate(*llvmModule, llvm::errs())) {
        llvm::errs() << "Settings could not be applied to the input llvmModule.\n";
        return nullptr;
    }

    auto frontend = std::make_unique<LLVMFrontend>(std::move(llvmModule), context, mSettings);

    // The Witness generator has to get the initial name of the sourcefile
    // ( witnesses support programs with a single source file only )
    if (!mSettings.witness.empty()) {
        if (inputs.size()!=1) {
            llvm::errs() << "Witnesses support programs with a single source file only. Gazer won't generate a witness, as there were more than one input files";
            mSettings.witness = "";
            mSettings.hash = "";
        } else { // TODO something more elegant?
            llvm::StringRef filename = llvm::sys::path::filename(inputs[0]);
            gazer::ViolationWitnessWriter::SourceFileName = filename;
            CorrectnessWitnessWriter::SourceFileName = filename;
        }
    }

    for (auto& check : checks) {
        // Release the unique pointer and add it to the check registry
        frontend->getChecks().add(check.release());
    }

    return frontend;
}

void FrontendConfig::createChecks(std::vector<std::unique_ptr<Check>>& checks)
{
    std::string filter = llvm::StringRef{mSettings.checks}.lower();
    llvm::SmallVector<llvm::StringRef, 8> fragments;
    if (filter.empty()) {
        // Do the defaults
        fragments.push_back("assertion-fail");
        fragments.push_back("div-by-zero");
    } else if (filter == AllChecksSetting) {
        // Add all registered checks
        for (auto& [name, factory] : mFactories) {
            fragments.push_back(name);
        }
    } else {
        // Split the string and determine what is enabled
        llvm::SplitString(filter, fragments, ",");
    }

    for (llvm::StringRef name : fragments) {
        auto it = mFactories.find(name);
        if (it == mFactories.end()) {
            emit_warning("unknown check '%s', parameter ignored", name.data());
            continue;
        }

        CheckFactory factory = it->second;
        checks.emplace_back(factory(mClangSettings));
    }
}

void FrontendConfigWrapper::PrintVersion(llvm::raw_ostream& os)
{
    os << "Gazer v" << GAZER_VERSION_STRING << "\nLLVM v" << LLVM_VERSION_STRING;
}
