#ifndef _GAZER_LLVM_INSTRUMENTATION_INTRINSICS_H
#define _GAZER_LLVM_INSTRUMENTATION_INTRINSICS_H

#include <llvm/IR/Instructions.h>
#include <llvm/IR/DebugInfoMetadata.h>

namespace gazer
{

class GazerIntrinsic
{
public:
    enum IntrinsicKind
    {
        InlinedGlobalWrite,
        FunctionEntry
    };
public:
    static llvm::Function* getDeclaration(IntrinsicKind kind, llvm::Module& module);

    static llvm::CallInst* CreateInlinedGlobalWrite(llvm::Value* value, llvm::DIGlobalVariable* gv);
    static llvm::CallInst* CreateFunctionEntry(llvm::Module& module, llvm::DISubprogram* dsp = nullptr);
};

}

#endif
