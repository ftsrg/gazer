//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2020 Contributors to the Gazer project
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
#include "gazer/LLVM/Instrumentation/DefaultChecks.h"

#include <llvm/IR/InstIterator.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace gazer;
using namespace llvm;

namespace
{

/// This check ensures that no assertion failure instructions are reachable.
class AssertionFailCheck final : public Check
{
public:
    static char ID;

    AssertionFailCheck()
        : Check(ID)
    {}

    bool mark(llvm::Function& function) override;

    llvm::StringRef getErrorDescription() const override { return "Assertion failure"; }
};

} // namespace

char AssertionFailCheck::ID;

static bool isCallToErrorFunction(llvm::Instruction& inst)
{
    auto call = llvm::dyn_cast<llvm::CallInst>(&inst);
    if (call == nullptr) {
        return false;
    }

    if (call->getCalledFunction() == nullptr) {
        return false;
    }

    auto name = call->getCalledFunction()->getName();

    return name == "__VERIFIER_error"
        || name == "__assert_fail"
        || name == "__gazer_error"
        || name == "reach_error";
}

bool AssertionFailCheck::mark(llvm::Function &function)
{
    return this->replaceMatchingUnreachableWithError(
        function, "error.assert_fail", isCallToErrorFunction
    );
}

std::unique_ptr<Check> gazer::checks::createAssertionFailCheck(ClangOptions& options)
{
    return std::make_unique<AssertionFailCheck>();
}
