//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
///
/// Processes special function behaviours
/// Currently for a given function name, asserts that the function is pure,
/// And transforms as such.
///
/// Example:
/// %1 := call @FixResult()
/// %2 := load <<type>>* %1
///
/// will become something like this:
///
/// @_FixResult = internal global <<type>> undef
/// %2 := load <<type>>* @_FixResult
///
//===----------------------------------------------------------------------===//

#ifndef SRC_LLVM_INSTRUMENTATION_ANNOTATESPECIALFUNCTIONS_H_
#define SRC_LLVM_INSTRUMENTATION_ANNOTATESPECIALFUNCTIONS_H_

#include <llvm/Pass.h>

namespace gazer {
llvm::Pass* createAnnotateSpecialFunctionsPass();
}

#endif /* SRC_LLVM_INSTRUMENTATION_ANNOTATESPECIALFUNCTIONS_H_ */
