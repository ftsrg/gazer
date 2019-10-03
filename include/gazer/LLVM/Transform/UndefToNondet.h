#ifndef GAZER_LLVM_TRANSFORM_UNDEFTONONDET_H
#define GAZER_LLVM_TRANSFORM_UNDEFTONONDET_H

#include <llvm/Pass.h>

namespace gazer
{

/// This pass turns undef values into nondetermistic functions calls,
/// forcing the optimizer to be more careful around undefined behavior.
class UndefToNondetCallPass : public llvm::ModulePass
{
public:
    static char ID;

    static constexpr char UndefValueFunctionPrefix[] = "gazer.undef_value.";

    UndefToNondetCallPass()
        : ModulePass(ID)
    {}

    bool runOnModule(llvm::Module& module) override;
};

}

#endif
