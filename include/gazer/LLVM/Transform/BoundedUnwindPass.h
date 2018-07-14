#ifndef _GAZER_LLVM_TRANSFORM_BOUNDEDUNWINDPASS_H
#define _GAZER_LLVM_TRANSFORM_BOUNDEDUNWINDPASS_H

#include <llvm/Pass.h>

namespace gazer
{

class BoundedUnwindPass : public llvm::FunctionPass
{
public:
    static char ID;

    BoundedUnwindPass(unsigned bound = 1);

    virtual void getAnalysisUsage(llvm::AnalysisUsage& au) const override;
    virtual bool runOnFunction(llvm::Function& function) override;
private:
    unsigned mBound;
};

llvm::Pass* createRemoveInfiniteLoopsPass();
llvm::Pass* createBoundedUnwindPass(unsigned bound);

}

#endif
