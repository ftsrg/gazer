#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include <llvm/IR/Module.h>

using namespace gazer;

llvm::Constant* GazerIntrinsic::GetOrInsertFunctionEntry(llvm::Module& module)
{
    return module.getOrInsertFunction(
        FunctionEntryName,
        llvm::Type::getVoidTy(module.getContext()),
        llvm::Type::getMetadataTy(module.getContext()),
        llvm::Type::getInt8Ty(module.getContext())
    );
}

llvm::Constant* GazerIntrinsic::GetOrInsertFunctionReturnVoid(llvm::Module& module)
{
    return module.getOrInsertFunction(
        FunctionReturnVoidName,
        llvm::Type::getVoidTy(module.getContext()),
        llvm::Type::getMetadataTy(module.getContext())
    );
}

llvm::Constant* GazerIntrinsic::GetOrInsertFunctionCallReturned(llvm::Module& module)
{
    return module.getOrInsertFunction(
        FunctionCallReturnedName,
        llvm::Type::getVoidTy(module.getContext()),
        llvm::Type::getMetadataTy(module.getContext())
    );
}

llvm::Constant* GazerIntrinsic::GetOrInsertFunctionReturnValue(llvm::Module& module, llvm::Type* type)
{
    assert(type != nullptr);

    std::string nameBuffer;
    llvm::raw_string_ostream rso(nameBuffer);
    type->print(rso, false, true);
    rso.flush();

    // Insert a new function for this mark type
    return module.getOrInsertFunction(
        "gazer.function.return_value." + rso.str(),
        llvm::Type::getVoidTy(module.getContext()),
        llvm::Type::getMetadataTy(module.getContext()),
        type
    );
}
