/**
 * @file SimplifySpecialFunctions.h
 *
 * @brief <<Write a short description of the file here. >>
 *
 * << Write the detailed description of the file here. >>
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
 * Simplifies special known, but not implemented functions.
 * 1) When certain conditions are met for a function,
 *       replaces function call with a pre-defined value
 * @return
 */
llvm::Pass* createSimplifySpecialFunctionsPass();
}

#endif /* SRC_LLVM_INSTRUMENTATION_SIMPLIFYSPECIALFUNCTIONS_H_ */
