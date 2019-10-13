#ifndef GAZER_CORE_TRANSFORM_PASSES_H
#define GAZER_CORE_TRANSFORM_PASSES_H

#include <llvm/Pass.h>

namespace gazer
{

/// InlineGlobalVariables - This pass inlines all global variables into
/// the main function of the program.
llvm::Pass* createInlineGlobalVariablesPass();

/// This pass combines each 'gazer.error_code' call within the function
/// into a single one.
llvm::Pass* createCombineErrorCallsPass();

llvm::Pass* createBackwardSlicerPass();

}

#endif
