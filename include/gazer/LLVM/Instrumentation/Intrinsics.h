/// \file This file describes the various intrinsic functions that
/// Gazer uses for instrumentation.
#ifndef _GAZER_LLVM_INSTRUMENTATION_INTRINSICS_H
#define _GAZER_LLVM_INSTRUMENTATION_INTRINSICS_H

#include <llvm/IR/Instructions.h>
#include <llvm/IR/DebugInfoMetadata.h>

#include <string_view>

namespace gazer
{

class GazerIntrinsic
{
public:
    static constexpr char FunctionEntryName[] = "gazer.function.entry";
    static constexpr char FunctionReturnVoidName[] = "gazer.function.return_void";
    static constexpr char FunctionCallReturnedName[] = "gazer.function.call_returned";
    static constexpr char FunctionReturnValuePrefix[] = "gazer.function.return_value.";

public:
    //static llvm::Function* getDeclaration(IntrinsicKind kind, llvm::Module& module);

    static llvm::CallInst* CreateInlinedGlobalWrite(llvm::Value* value, llvm::DIGlobalVariable* gv);
    static llvm::CallInst* CreateFunctionEntry(llvm::Module& module, llvm::DISubprogram* dsp = nullptr);


public:
    /// Returns a 'gazer.function.entry(metadata fn_name, i8 num_args)' intrinsic.
    static llvm::FunctionCallee GetOrInsertFunctionEntry(llvm::Module& module);

    /// Returns a 'gazer.function.return_void(metadata fn_name)' intrinsic.
    static llvm::FunctionCallee GetOrInsertFunctionReturnVoid(llvm::Module& module);

    /// Returns a 'gazer.function.call_returned(metadata fn_name)' intrinsic.
    static llvm::FunctionCallee GetOrInsertFunctionCallReturned(llvm::Module& module);

    /// Returns a 'gazer.function.return_value.T(metadata fn_name, T retval)' intrinsic,
    /// where 'T' is the given return type.
    static llvm::FunctionCallee GetOrInsertFunctionReturnValue(llvm::Module& module, llvm::Type* type);
};

}

#endif
