#include "gazer/Core/Type.h"

#include <llvm/IR/Type.h>
#include <llvm/IR/DerivedTypes.h>

namespace gazer
{

llvm::Type* llvmTypeFromType(llvm::LLVMContext& context, const gazer::Type& type);

}
