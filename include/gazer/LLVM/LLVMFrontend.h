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
#ifndef GAZER_LLVM_LLVMFRONTEND_H
#define GAZER_LLVM_LLVMFRONTEND_H

#include "gazer/LLVM/ClangFrontend.h"
#include "gazer/LLVM/Instrumentation/Check.h"
#include "gazer/LLVM/LLVMFrontendSettings.h"
#include "gazer/Verifier/VerificationAlgorithm.h"

#include <llvm/Pass.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/ToolOutputFile.h>

namespace gazer
{

class GazerContext;
class LLVMFrontend;

/// Builder class for LLVM frontends.
class FrontendConfig
{
public:
    using CheckFactory = std::function<std::unique_ptr<Check>(ClangOptions&)>;

    static constexpr char AllChecksSetting[] = "all";
public:
    FrontendConfig();

    /// Inserts a given check into the system. Inserted checks may be disabled
    /// through the command-line. The check factory should be responsible for
    /// creating the check object and inserting relevant flags into the
    /// provided ClangOptions object. The factory will be executed (and the
    /// flags will be inserted) only if the corresponding check was enabled.
    void registerCheck(llvm::StringRef name, CheckFactory factory);

    template<class T>
    void registerCheck(llvm::StringRef name)
    {
        static_assert(std::is_base_of_v<Check, T>, "Registered checks must inherit from Check!");
        addCheck(name, []() { return std::make_unique<T>(); });
    }

    std::unique_ptr<LLVMFrontend> buildFrontend(
        llvm::ArrayRef<std::string> inputs,
        GazerContext& context,
        llvm::LLVMContext& llvmContext
    );

    LLVMFrontendSettings& getSettings() { return mSettings; }

private:
    void createChecks(std::vector<std::unique_ptr<Check>>& checks);

private:
    ClangOptions mClangSettings;
    LLVMFrontendSettings mSettings;
    std::map<std::string, CheckFactory> mFactories;
};

/// A convenience frontend object which sets up command-line arguments
/// and performs some clean-up on destruction.
class FrontendConfigWrapper
{
public:
    static void PrintVersion(llvm::raw_ostream& os);

    FrontendConfigWrapper() = default;

    std::unique_ptr<LLVMFrontend> buildFrontend(llvm::ArrayRef<std::string> inputs)
    {
        return config.buildFrontend(inputs, context, llvmContext);
    }

    LLVMFrontendSettings& getSettings() { return config.getSettings(); }

private:
    llvm::llvm_shutdown_obj mShutdown; // This should be kept as first, will be destroyed last
public:
    llvm::LLVMContext llvmContext;
    GazerContext context;
    FrontendConfig config;
};

class LLVMFrontend
{
public:
    LLVMFrontend(
        std::unique_ptr<llvm::Module> llvmModule,
        GazerContext& context,
        LLVMFrontendSettings& settings
    );

    LLVMFrontend(const LLVMFrontend&) = delete;
    LLVMFrontend& operator=(const LLVMFrontend&) = delete;

    static std::unique_ptr<LLVMFrontend> FromInputFile(
        llvm::StringRef input,
        GazerContext& context,
        llvm::LLVMContext& llvmContext,
        LLVMFrontendSettings& settings
    );

    /// Registers the common preprocessing analyses and transforms of the 
    /// verification pipeline into the pass manager. After executing the
    /// registered passes, the input LLVM module will be optimized, and the
    /// translated automata system will be available, and if there is a set
    /// backend algorithm, it will be run.
    void registerVerificationPipeline();

    /// Registers an arbitrary pass into the pipeline.
    void registerPass(llvm::Pass* pass);

    /// Sets the backend algorithm to be used in the verification process.
    /// The LLVMFrontend instance will take ownership of the backend object.
    /// Note: this function *must* be called before `registerVerificationPipeline`!
    void setBackendAlgorithm(VerificationAlgorithm* backend)
    {
        assert(mBackendAlgorithm == nullptr && "Can register only one backend algorithm!");
        mBackendAlgorithm.reset(backend);
    }

    /// Runs the registered LLVM pass pipeline.
    void run();

    CheckRegistry& getChecks() { return mChecks; }
    LLVMFrontendSettings& getSettings() { return mSettings; }

    GazerContext& getContext() const { return mContext; }
    llvm::Module& getModule() const { return *mModule; }
private:
    //---------------------- Individual pipeline steps ---------------------//
    void registerEnabledChecks();
    void registerEarlyOptimizations();
    void registerLateOptimizations();
    void registerInlining();
    void registerVerificationStep();

private:
    GazerContext& mContext;
    std::unique_ptr<llvm::Module> mModule;

    CheckRegistry mChecks;
    llvm::legacy::PassManager mPassManager;

    LLVMFrontendSettings& mSettings;
    std::unique_ptr<VerificationAlgorithm> mBackendAlgorithm = nullptr; 

    std::unique_ptr<llvm::ToolOutputFile> mModuleOutput = nullptr;
};

}

#endif