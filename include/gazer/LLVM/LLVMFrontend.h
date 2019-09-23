#ifndef GAZER_LLVM_LLVMFRONTEND_H
#define GAZER_LLVM_LLVMFRONTEND_H

#include "gazer/LLVM/Instrumentation/Check.h"

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
        GazerContext& context
    );

    static std::unique_ptr<LLVMFrontend> FromInputFile(
        llvm::StringRef input,
        GazerContext& context,
        llvm::LLVMContext& llvmContext
    );

    /// Registers the common preprocessing analyses and transforms of the 
    /// verification pipeline into the pass manager. After executing the
    /// registered passes, the input LLVM module will be reduced, and the
    /// translated automata system will be available.
    void registerVerificationPipeline();

    /// Register an arbitrary pass into the pipeline.
    void registerPass(llvm::Pass* pass);

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
    void registerInliningIfEnabled();
    void registerAutomataTranslation();

private:
    GazerContext& mContext;
    std::unique_ptr<llvm::Module> mModule;

    CheckRegistry mChecks;
    llvm::legacy::PassManager mPassManager;
};

}

#endif