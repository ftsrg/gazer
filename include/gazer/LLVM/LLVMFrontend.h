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

#include "gazer/LLVM/Instrumentation/Check.h"
#include "gazer/LLVM/LLVMFrontendSettings.h"
#include "gazer/Verifier/VerificationAlgorithm.h"

#include <llvm/Pass.h>
#include <llvm/IR/LegacyPassManager.h>

namespace gazer
{

class GazerContext;

class LLVMFrontend
{
public:
    LLVMFrontend(
        std::unique_ptr<llvm::Module> module,
        GazerContext& context,
        LLVMFrontendSettings settings
    );

    LLVMFrontend(const LLVMFrontend&) = delete;
    LLVMFrontend& operator=(const LLVMFrontend&) = delete;

    static std::unique_ptr<LLVMFrontend> FromInputFile(
        llvm::StringRef input,
        GazerContext& context,
        llvm::LLVMContext& llvmContext,
        LLVMFrontendSettings settings
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
        assert(mBackendAlgorithm == nullptr && "Can only register one backend algorithm!");
        mBackendAlgorithm.reset(backend);
    }

    /// Runs the registered LLVM pass pipeline.
    void run();

    GazerContext& getContext() const { return mContext; }
    const CheckRegistry& getChecks() const { return mChecks; }
    llvm::Module& getModule() const { return *mModule; }

private:
    //---------------------- Individual pipeline steps ---------------------//
    void registerEnabledChecks();
    void registerEarlyOptimizations();
    void registerLateOptimizations();
    void registerInlining();

private:
    GazerContext& mContext;
    std::unique_ptr<llvm::Module> mModule;

    CheckRegistry mChecks;
    llvm::legacy::PassManager mPassManager;

    LLVMFrontendSettings mSettings;
    std::unique_ptr<VerificationAlgorithm> mBackendAlgorithm = nullptr; 
};

}

#endif