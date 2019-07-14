#include "gazer/LLVM/Analysis/MemoryObject.h"

using namespace gazer;

// LLVM pass implementation
//-----------------------------------------------------------------------------

char MemoryObjectPass::ID;

bool MemoryObjectPass::runOnFunction(llvm::Function& module)
{
    return false;
}