#ifndef _GAZER_LLVM_ANALYSIS_BMCPASS_H
#define _GAZER_LLVM_ANALYSIS_BMCPASS_H


#include <llvm/Pass.h>

namespace gazer
{

class BmcPass final : public llvm::FunctionPass
{
public:
    static char ID;

    BmcPass(unsigned bound)
        : FunctionPass(ID), mBound(bound)
    {}

    virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const override;
    virtual bool runOnFunction(llvm::Function& function) override;
private:
    unsigned mBound;
};

}

#endif
