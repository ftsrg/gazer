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

#include <llvm/IR/Module.h>
#include <llvm/ADT/StringExtras.h>

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
    mFactories[name] = factory;
}

auto FrontendConfig::buildFrontend(
    llvm::ArrayRef<std::string> inputs,
    GazerContext& context,
    llvm::LLVMContext& llvmContext
) -> std::unique_ptr<LLVMFrontend>
{
    // FIXME: if something fails below this code, the checks will leak.
    std::vector<Check*> checks;
    createChecks(checks);

    auto module = ClangCompileAndLink(inputs, llvmContext, mClangSettings);
    if (module == nullptr) {
        return nullptr;
    }

    auto frontend = std::make_unique<LLVMFrontend>(std::move(module), context, mSettings);

    for (Check* check : checks) {
        frontend->getChecks().add(check);
    }

    return frontend;
}

void FrontendConfig::createChecks(std::vector<Check*>& checks)
{
    std::string filter = llvm::StringRef{mSettings.checks}.lower();
    llvm::SmallVector<llvm::StringRef, 8> fragments;
    if (filter.empty()) {
        // Do the defaults
        fragments.push_back("assertion-fail");
        fragments.push_back("div-by-zero");
        fragments.push_back("signed-overflow");
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
        CheckFactory factory = mFactories[name];
        checks.push_back(factory(mClangSettings));
    }
}

void FrontendConfigWrapper::PrintVersion(llvm::raw_ostream& os)
{
    os << "gazer - a formal verification frontend\n";
    os << "   version 0.1\n";
    os << "   LLVM version 9.0\n";
}
