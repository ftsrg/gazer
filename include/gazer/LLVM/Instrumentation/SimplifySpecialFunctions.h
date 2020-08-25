/**
 * @file SimplifySpecialFunctions.h
 *
 * @brief Contains pass which simplifies functions with special properties
 *
 * Simplifies functions with special properties so analysis can take place.
 * There are some hard to analyze constructs like pointer handling.
 * The goal of this pass is to rewrite the function call to easier constructs.
 * 
 * When certain conditions are met for a function,
 *   replaces function call with access to a global variable
 *
 * @author laszlo.radnai
 * @date 2020.08.19.
 * 
 * @copyright 2020 thyssenkrupp Components Technology Hungary Ltd.
 */

#ifndef SRC_LLVM_INSTRUMENTATION_SIMPLIFYSPECIALFUNCTIONS_H_
#define SRC_LLVM_INSTRUMENTATION_SIMPLIFYSPECIALFUNCTIONS_H_

#include <memory>

namespace llvm {
class Pass;
}

namespace gazer {
/**
 * Simplifies functions with special properties so analysis can take place.
 * There are some hard to analyze constructs like pointer handling.
 * The goal of this pass is to rewrite the function call to easier constructs.
 *
 * Simplifies special known, but not implemented functions.
 * When certain conditions are met for a function,
 *   replaces function call with access to a global variable
 * @return
 */
llvm::Pass* createSimplifySpecialFunctionsPass();
}

#endif /* SRC_LLVM_INSTRUMENTATION_SIMPLIFYSPECIALFUNCTIONS_H_ */
