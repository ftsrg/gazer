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

#include <unordered_set>

using namespace llvm;

// Implemented by gazer-theta-re.cpp :'(
// TODO This is ugly
__attribute__((weak)) std::vector<std::string> getAnnotationsFor(std::string functionName) {
    return {};
}
//extern std::vector<std::string> getAnnotationsFor(std::string functionName);

namespace {

class AnnotateSpecialFunctionsPass : public ModulePass {
    std::unordered_set<Function*> fixptrs;
public:
    AnnotateSpecialFunctionsPass() : ModulePass(ID) {}

    bool runOnModule(llvm::Module& m) override {
        for (auto& f: m.functions()) {
            processFunction(f);
        }
        return true;
    }

    bool processFunction(Function& func) {
        for (std::string annot : getAnnotationsFor(func.getName())) {
            if (annot == "fixptr") {
                bool ok = true;
                if (func.arg_size() != 0) {
                    func.getContext().emitError("FixPtr calls must have zero parameters\n");
                    ok = false;
                    // do not return for more checks
                }
                if (!func.getFunctionType()->getReturnType()->isPointerTy()) {
                    func.getContext().emitError("FixPtr calls must have pointer return type\n");
                    ok = false;
                }
                if (!ok) {
                    // no change
                    return false;
                }

                //fixptrs.insert(&func);

                func.setSpeculatable();
                func.setDoesNotAccessMemory();
                func.setReturnDoesNotAlias();
                return true; // TODO what happens with multiple annotations on a single call?
            }
        }
        return false;
    }

public:
    static char ID;

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
