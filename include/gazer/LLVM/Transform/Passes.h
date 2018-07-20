#ifndef _GAZER_CORE_TRANSFORM_PASSES_H
#define _GAZER_CORE_TRANSFORM_PASSES_H

#include <llvm/Pass.h>

namespace gazer
{

llvm::Pass* createInlineGlobalVariablesPass();

llvm::Pass* createBackwardSlicerPass();

llvm::Pass* createCombineErrorCallsPass();

}

#endif
