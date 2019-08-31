#ifndef GAZER_LLVM_TYPETRANSLATOR_H
#define GAZER_LLVM_TYPETRANSLATOR_H

#include "gazer/Core/Type.h"

#include <llvm/IR/Type.h>
#include "llvm/IR/DerivedTypes.h"

namespace gazer
{

class MemoryModel;

class LLVMTypeTranslator final
{
public:
    LLVMTypeTranslator(MemoryModel& memoryModel, bool intsAsBv = true);

    gazer::Type& get(const llvm::Type* type);

protected:
    MemoryModel& mMemoryModel;
    bool mIntsAsBv;
};

}

#endif