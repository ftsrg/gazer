#ifndef _GAZER_LLVM_CFABUILDERPASS_H
#define _GAZER_LLVM_CFABUILDERPASS_H

#include "gazer/Core/Automaton.h"

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

    Automaton& getCFA() { return *mCFA; }

    virtual bool runOnFunction(llvm::Function& function) override;

private:
    std::unique_ptr<Automaton> mCFA;
};

}

#endif
