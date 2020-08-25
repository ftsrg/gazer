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

#include "gazer/LLVM/Instrumentation/AnnotateSpecialFunctions.h"
#include "llvm/Support/Casting.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

namespace {

// TODO use this only to mark functions, add a Checks pass that watch for the attributes/marked functions
class AnnotateSpecialFunctionsPass : public ModulePass {
public:
    AnnotateSpecialFunctionsPass() : ModulePass(ID) {}

    bool runOnModule(Module& m) override {
        bool res = false;
        SmallVector<CallInst*, 16> toProcess;
        for (auto& f : m.functions()) {
            for (auto& bb: f) {
                for (auto& inst : bb) {
                    if (auto* callInst = dyn_cast<CallInst>(&inst)) {
                        toProcess.push_back(callInst);
                    }
                }
            }
        }
        for (auto* callInst : toProcess) {
            res |= processInstruction(m, *callInst);
        }
        return res;
    }

public:
    static char ID;

private:

    bool processInstruction(Module& m, CallInst& inst) {
        auto enterExitCounterType = Type::getInt32Ty(m.getContext());
        // known API functions
        if (inst.getCalledFunction()->getName().startswith("FixPtr")) {
            auto varName = ("gazer." + inst.getCalledFunction()->getName()).str();

            bool ok = true;
            if (inst.getNumArgOperands() != 0) {
                m.getContext().emitError(&inst, "FixPtr calls must have zero parameters\n");
                ok = false;
                // do not return for more checks
            }
            if (!inst.getFunctionType()->getReturnType()->isPointerTy()) {
                m.getContext().emitError(&inst, "FixPtr calls must have pointer return type\n");
                ok = false;
            }
            if (!ok) {
                // no change
                return false;
            }

            inst.setDoesNotAccessMemory();
            inst.getCalledFunction()->setSpeculatable();
            inst.getCalledFunction()->setDoesNotAccessMemory();
            inst.getCalledFunction()->setReturnDoesNotAlias();
            return true;
        }
        // known intrinsics
        if (inst.getCalledFunction()->getName().startswith("gazer.") ||
                inst.getCalledFunction()->getName().startswith("llvm.")) {
            return false;
        }
        errs() << "WARNING: unknown function " << *(inst.getCalledFunction()) << "\n";
        return false;
    }

};
}

char AnnotateSpecialFunctionsPass::ID;

char& gazer::getAnnotateSpecialFunctionsID() {
    return AnnotateSpecialFunctionsPass::ID;
}

static RegisterPass<AnnotateSpecialFunctionsPass> X("annot-spec-functions", "annot-spec-functions", false, true);
/*
llvm::Pass* gazer::createAnnotateSpecialFunctionsPass() {
    return new AnnotateSpecialFunctionsPass();
}
*/
