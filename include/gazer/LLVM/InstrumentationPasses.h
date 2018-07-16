#ifndef _GAZER_LLVM_INSTRUMENTATIONPASSES_H
#define _GAZER_LLVM_INSTRUMENTATIONPASSES_H

#include <llvm/Pass.h>

namespace gazer
{

llvm::Pass* createPromoteUndefsPass();

}

#endif
