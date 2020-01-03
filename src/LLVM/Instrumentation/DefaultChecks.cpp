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

#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

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
public:
    static char ID;

    SignedIntegerOverflowCheck()
        : Check(ID)
    {}
    
    bool mark(llvm::Function& function) override
    {
        return false;
    }
};

char DivisionByZeroCheck::ID;
char AssertionFailCheck::ID;
char SignedIntegerOverflowCheck::ID;

} // end anonymous namespace

Check* gazer::checks::createDivisionByZeroCheck() {
    return new DivisionByZeroCheck();
}

Check* gazer::checks::createAssertionFailCheck() {
    return new AssertionFailCheck();
}
