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
#include "gazer/LLVM/Instrumentation/Intrinsics.h"
#include "gazer/ADT/Iterator.h"
#include "gazer/Support/Warnings.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Regex.h>
#include <llvm/ADT/StringSwitch.h>

using namespace gazer;
using namespace llvm;

namespace
{

class SignedIntegerOverflowCheck final : public Check
{
    static constexpr char IntrinsicPattern[] = "^llvm\\.(u|s)(add|sub|mul)\\.with\\.overflow\\.i([0-9]+)$";
public:
    static char ID;

    SignedIntegerOverflowCheck()
        : Check(ID), mIntrinsicRegex(IntrinsicPattern)
    {}

    bool mark(llvm::Function& function) override;

    StringRef getErrorDescription() const override
    {
        return "Signed integer overflow";
    }
private:
    bool isOverflowIntrinsic(llvm::Function* callee, GazerIntrinsic::Overflow* overflowKind);

private:
    llvm::Regex mIntrinsicRegex;
};

} // namespace

char SignedIntegerOverflowCheck::ID;

bool SignedIntegerOverflowCheck::isOverflowIntrinsic(
    llvm::Function* callee,
    GazerIntrinsic::Overflow* overflowKind)
{
    if (callee == nullptr) {
        return false;
    }

    // [ input, signedness, op, width ]
    llvm::SmallVector<llvm::StringRef, 4> groups;
    if (!mIntrinsicRegex.match(callee->getName(), &groups)) {
        return false;
    }

    // Determine signedness
    bool isSigned;
    if (groups[1] == "s") {
        isSigned = true;
    } else if (groups[1] == "u") {
        isSigned = false;
    } else {
        llvm_unreachable("Unknown overflow intrinsic signedness!");
    }

    llvm::StringRef op = groups[2];
    if (isSigned) {
        *overflowKind = llvm::StringSwitch<GazerIntrinsic::Overflow>(op)
            .Case("add", GazerIntrinsic::Overflow::SAdd)
            .Case("sub", GazerIntrinsic::Overflow::SSub)
            .Case("mul", GazerIntrinsic::Overflow::SMul);
    } else {
        *overflowKind = llvm::StringSwitch<GazerIntrinsic::Overflow>(op)
            .Case("add", GazerIntrinsic::Overflow::UAdd)
            .Case("sub", GazerIntrinsic::Overflow::USub)
            .Case("mul", GazerIntrinsic::Overflow::UMul);
    }

    return true;
}

static bool isOverflowTrapCall(llvm::Instruction& inst)
{
    auto call = llvm::dyn_cast<llvm::CallInst>(&inst);
    if (call == nullptr) {
        return false;
    }

    auto callee = call->getCalledFunction();
    if (callee == nullptr) {
        return false;
    }

    auto name = callee->getName();

    return name == "__ubsan_handle_add_overflow_abort"
        || name == "__ubsan_handle_sub_overflow_abort"
        || name == "__ubsan_handle_mul_overflow_abort";
}

bool SignedIntegerOverflowCheck::mark(llvm::Function &function)
{
    bool modified = false;

    llvm::Module& module = *function.getParent();
    llvm::IRBuilder<> builder(module.getContext());

    llvm::SmallVector<std::pair<llvm::CallInst*, GazerIntrinsic::Overflow>, 16> targets;
    llvm::SmallVector<llvm::CallInst*, 16> sanitizerCalls;

    for (auto& call : classof_range<llvm::CallInst>(llvm::instructions(function))) {
        GazerIntrinsic::Overflow ovrKind;
        if (this->isOverflowIntrinsic(call.getCalledFunction(), &ovrKind)) {
            targets.emplace_back(&call, ovrKind);
        }
    }

    // A call to `llvm.*.with.overflow.iN` returns a {iN, i1} pair where the
    // second element is the flag which tells us whether an overflow has occured.
    // As such, we will replace all uses of the second element with our own overflow
    // check, and all uses of the first element with the actual operation.
    for (auto& [call, ovrKind] : targets) {
        llvm::Type* valTy = call->getType()->getStructElementType(0);

        auto lhs = call->getArgOperand(0);
        auto rhs = call->getArgOperand(1);

        auto overflowCheckFn = GazerIntrinsic::GetOrInsertOverflowCheck(module, ovrKind, valTy);

        builder.SetInsertPoint(call);
        auto check = builder.CreateCall(overflowCheckFn, { lhs, rhs }, "ovr_check");
        auto checkFail = builder.CreateNot(check);

        llvm::Value* binOp = nullptr;
        switch (ovrKind) {
            case GazerIntrinsic::Overflow::SAdd: binOp = builder.CreateAdd(lhs, rhs, "", /*HasNUW=*/false, /*HasNSW=*/true); break;
            case GazerIntrinsic::Overflow::SSub: binOp = builder.CreateSub(lhs, rhs, "", /*HasNUW=*/false, /*HasNSW=*/true); break;
            case GazerIntrinsic::Overflow::SMul: binOp = builder.CreateMul(lhs, rhs, "", /*HasNUW=*/false, /*HasNSW=*/true); break;
            case GazerIntrinsic::Overflow::UAdd: binOp = builder.CreateAdd(lhs, rhs, "", /*HasNUW=*/true,  /*HasNSW=*/false); break;
            case GazerIntrinsic::Overflow::USub: binOp = builder.CreateSub(lhs, rhs, "", /*HasNUW=*/true,  /*HasNSW=*/false); break;
            case GazerIntrinsic::Overflow::UMul: binOp = builder.CreateMul(lhs, rhs, "", /*HasNUW=*/true,  /*HasNSW=*/false); break;
        }

        assert(binOp != nullptr && "Unknown overflow kind!");

        // Check the uses of 'call'
        for (auto ui = call->user_begin(), ue = call->user_end(); ui != ue;) {
            auto current = ui++;
            if (auto extract = llvm::dyn_cast<llvm::ExtractValueInst>(*current)) {
                unsigned firstIdx = extract->getIndices()[0];
                if (firstIdx == 0) {
                    extract->replaceAllUsesWith(binOp);
                } else if (firstIdx == 1) {
                    extract->replaceAllUsesWith(checkFail);
                } else {
                    llvm_unreachable("Unknown overflow struct index!");
                }

                extract->dropAllReferences();
                extract->eraseFromParent();
            }
        }

        call->dropAllReferences();
        call->eraseFromParent();

        modified = true;
    }

    // Find trap calls and replace them with our own error block
    modified |= this->replaceMatchingUnreachableWithError(function, "error.signed_overflow", isOverflowTrapCall);

    return modified;
}

std::unique_ptr<Check> gazer::checks::createSignedIntegerOverflowCheck(ClangOptions& options)
{
    options.addSanitizerFlag("signed-integer-overflow");
    return std::make_unique<SignedIntegerOverflowCheck>();
}

