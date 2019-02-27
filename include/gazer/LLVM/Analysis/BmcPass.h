#ifndef _GAZER_LLVM_ANALYSIS_BMCPASS_H
#define _GAZER_LLVM_ANALYSIS_BMCPASS_H

#include <llvm/Pass.h>

namespace gazer
{

class BmcPass final : public llvm::FunctionPass
{
public:
    static char ID;

    BmcPass()
        : FunctionPass(ID)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override;
    bool runOnFunction(llvm::Function& function) override;

    llvm::StringRef getPassName() const override {
        return "Bounded model cheking";
    }

private:
};

}

#endif
