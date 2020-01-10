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
#include "gazer/LLVM/Instrumentation/DefaultChecks.h"
#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Support/Regex.h>

using namespace gazer;
using namespace llvm;

namespace
{

bool isErrorFunctionName(llvm::StringRef name)
{
    return name == "__VERIFIER_error"
        || name == "__assert_fail"
        || name == "__gazer_error";
}

/// This check ensures that no assertion failure instructions are reachable.
class AssertionFailCheck final : public Check
{
public:
    static char ID;

    AssertionFailCheck()
        : Check(ID)
    {}
    
    bool mark(llvm::Function& function) override
    {
        for (BasicBlock& bb : function) {
            auto it = bb.begin();
            while (it != bb.end()) {
                if (it->getOpcode() == llvm::Instruction::Call) {
                    auto call = llvm::dyn_cast<llvm::CallInst>(&*it);
                    llvm::Function* callee = call->getCalledFunction();
                    if (callee == nullptr) {
                        ++it;
                        continue;
                    }

                    // Replace error calls with an unconditional jump
                    // to an error block
                    if (isErrorFunctionName(callee->getName())) {
                        BasicBlock* errorBB = this->createErrorBlock(
                            function,
                            "error.assert_fail",
                            call
                        );
                        
                        llvm::ReplaceInstWithInst(
                            bb.getTerminator(),
                            llvm::BranchInst::Create(errorBB)
                        );
                        call->eraseFromParent();

                        break;
                    }
                }

                ++it;
            }
        }

        // TODO: Try to preserve some analyses?
        return true;
    }

    llvm::StringRef getCheckName() const override { return "assert-fail"; }
    llvm::StringRef getErrorDescription() const override { return "Assertion failure"; }
};

bool isDiv(unsigned opcode) {
    return opcode == Instruction::SDiv || opcode == Instruction::UDiv;
}

/// Checks for division by zero errors.
class DivisionByZeroCheck final : public Check
{
public:
    static char ID;

    DivisionByZeroCheck()
        : Check(ID)
    {}

    bool mark(llvm::Function& function) override
    {
        auto& context = function.getContext();

        std::vector<llvm::Instruction*> divs;
        for (llvm::Instruction& inst : instructions(function)) {
            if (isDiv(inst.getOpcode())) {
                divs.push_back(&inst);
            }
        }

        if (divs.empty()) {
            return false;
        }

        unsigned divCnt = 0;
        IRBuilder<> builder(context);
        for (llvm::Instruction* inst : divs) {
            BasicBlock* errorBB = this->createErrorBlock(
                function,
                "error.divzero" + llvm::Twine(divCnt++),
                inst
            );

            BasicBlock* bb = inst->getParent();
            llvm::Value* rhs = inst->getOperand(1);

            builder.SetInsertPoint(inst);
            auto icmp = builder.CreateICmpNE(
                rhs, builder.getInt(llvm::APInt(
                    rhs->getType()->getIntegerBitWidth(), 0
                ))
            );

            BasicBlock* newBB = bb->splitBasicBlock(inst);
            builder.ClearInsertionPoint();
            llvm::ReplaceInstWithInst(
                bb->getTerminator(),
                builder.CreateCondBr(icmp, newBB, errorBB)
            );
        }

        return true;
    }

    llvm::StringRef getCheckName() const override { return "div-by-zero"; }
    llvm::StringRef getErrorDescription() const override { return "Divison by zero"; }
};

/// Checks for over- and underflow in signed integer operations.
class SignedIntegerOverflowCheck : public Check
{
    static constexpr char IntrinsicPattern[] = "^llvm.(u|s)(add|sub|mul).with.overflow.i([0-9]+)$";
public:
    static char ID;

    SignedIntegerOverflowCheck()
        : Check(ID), mIntrinsicRegex(IntrinsicPattern)
    {}
    
    bool mark(llvm::Function& function) override;

    llvm::StringRef getCheckName() const override { return "overflow"; }
    llvm::StringRef getErrorDescription() const override { return "Signed integer overflow"; }
private:
    bool isOverflowIntrinsic(llvm::Function* callee, GazerIntrinsic::Overflow* ovrKind);
    bool isSanitizerCall(llvm::Function* callee);

private:
    llvm::Regex mIntrinsicRegex;
};

char DivisionByZeroCheck::ID;
char AssertionFailCheck::ID;
char SignedIntegerOverflowCheck::ID;

} // end anonymous namespace

bool SignedIntegerOverflowCheck::isOverflowIntrinsic(llvm::Function* callee, GazerIntrinsic::Overflow* ovrKind)
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
    if (op == "add") {
        *ovrKind = isSigned ? GazerIntrinsic::Overflow::SAdd : GazerIntrinsic::Overflow::UAdd;
    } else if (op == "sub") {
        *ovrKind = isSigned ? GazerIntrinsic::Overflow::SSub : GazerIntrinsic::Overflow::USub;
    } else if (op == "mul") {
        *ovrKind = isSigned ? GazerIntrinsic::Overflow::SMul : GazerIntrinsic::Overflow::UMul;
    } else {
        llvm_unreachable("Unknown overflow intrinsic operation!");
    }

    return true;
}

bool SignedIntegerOverflowCheck::mark(llvm::Function& function)
{
    llvm::Module& module = *function.getParent();
    llvm::IRBuilder<> builder(module.getContext());

    llvm::SmallVector<std::pair<llvm::CallInst*, GazerIntrinsic::Overflow>, 16> targets;
    llvm::SmallVector<llvm::CallInst*, 16> sanitizerCalls;

    for (Instruction& inst : instructions(function)) {
        if (auto call = dyn_cast<CallInst>(&inst)) {
            GazerIntrinsic::Overflow ovrKind;
            if (this->isOverflowIntrinsic(call->getCalledFunction(), &ovrKind)) {
                targets.emplace_back(call, ovrKind);                    
            }
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

        auto fn = GazerIntrinsic::GetOrInsertOverflowCheck(module, ovrKind, valTy);

        builder.SetInsertPoint(call);
        auto check = builder.CreateCall(fn, { lhs, rhs }, "ovr_check");

        llvm::Value* binOp;
        switch (ovrKind) {
            case GazerIntrinsic::Overflow::SAdd: binOp = builder.CreateAdd(lhs, rhs, "", /*nuw=*/false, /*nsw=*/true); break;
            case GazerIntrinsic::Overflow::SSub: binOp = builder.CreateSub(lhs, rhs, "", /*nuw=*/false, /*nsw=*/true); break;
            case GazerIntrinsic::Overflow::SMul: binOp = builder.CreateMul(lhs, rhs, "", /*nuw=*/false, /*nsw=*/true); break;
            case GazerIntrinsic::Overflow::UAdd: binOp = builder.CreateAdd(lhs, rhs, "", /*nuw=*/true, /*nsw=*/false); break;
            case GazerIntrinsic::Overflow::USub: binOp = builder.CreateSub(lhs, rhs, "", /*nuw=*/true, /*nsw=*/false); break;
            case GazerIntrinsic::Overflow::UMul: binOp = builder.CreateMul(lhs, rhs, "", /*nuw=*/true, /*nsw=*/false); break;
        }

        assert(binOp != nullptr && "Unknown overflow kind!");

        BasicBlock* bb = call->getParent();
        BasicBlock* errorBB = this->createErrorBlock(function, "error.ovr", call);

        BasicBlock* newBB = bb->splitBasicBlock(llvm::cast<Instruction>(binOp));
        builder.ClearInsertionPoint();
        llvm::ReplaceInstWithInst(
            bb->getTerminator(),
            builder.CreateCondBr(check, newBB, errorBB)
        );

        // Clean up the original sanitizer instrumentation

        auto ui = call->user_begin();
        auto ue = call->user_end();

        while (ui != ue) {
            auto curr = ui++;
            if (auto extract = dyn_cast<ExtractValueInst>(*curr)) {
                unsigned index = extract->getIndices()[0];
                if (index == 0) {
                    extract->replaceAllUsesWith(binOp);
                } else if (index == 1) {
                    extract->replaceAllUsesWith(check);
                } else {
                    llvm_unreachable("Unknown index for a { iN, i1 } struct!");
                }
                extract->dropAllReferences();
                extract->eraseFromParent();
            }
        }

        call->dropAllReferences();
        call->eraseFromParent();
    }

    return true;
}

Check* gazer::checks::createDivisionByZeroCheck() { return new DivisionByZeroCheck(); }
Check* gazer::checks::createAssertionFailCheck() { return new AssertionFailCheck(); }
Check* gazer::checks::createSignedIntegerOverflowCheck() { return new SignedIntegerOverflowCheck(); }
