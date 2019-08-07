#ifndef GAZER_LLVM_VERIFIER_BMCPASS_H
#define GAZER_LLVM_VERIFIER_BMCPASS_H

#include "gazer/Verifier/BoundedModelChecker.h"

#include <llvm/Pass.h>

namespace gazer
{

class CheckRegistry;

class BoundedModelCheckerPass final : public llvm::ModulePass
{
public:
    static char ID;

    BoundedModelCheckerPass(CheckRegistry& checks)
        : ModulePass(ID), mChecks(checks)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override;
    bool runOnModule(llvm::Module& module) override;

    llvm::StringRef getPassName() const override {
        return "Bounded model checking";
    }

private:
    CheckRegistry& mChecks;
    std::unique_ptr<SafetyResult> mResult;
};

}

#endif
