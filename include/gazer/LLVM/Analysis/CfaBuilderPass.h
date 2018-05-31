#ifndef _GAZER_LLVM_CFABUILDERPASS_H
#define _GAZER_LLVM_CFABUILDERPASS_H

#include <llvm/Pass.h>

namespace gazer
{

class CfaBuilderPass final : public llvm::FunctionPass
{
public:
    static char ID;

    CfaBuilderPass()
        : FunctionPass(ID)
    {}

    virtual bool runOnFunction(llvm::Function& function) override;

private:
    llvm::DenseMap<BasicBlock*, std::pair<Location*, Location*>> mMap;
};

}

#endif
