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
/// Simplifies special functions
/// If a function:
///   return value is a pointer
///   the return pointer does not alias
///   has no arguments
///   does not access memory,
///   and speculatable
/// it can be replaced with a global variable access.

#include "gazer/LLVM/Instrumentation/SimplifySpecialFunctions.h"
#include "gazer/LLVM/Instrumentation/AnnotateSpecialFunctions.h"
#include "gazer/LLVM/Instrumentation/Check.h"

#include "llvm/Pass.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"

using namespace gazer;
using namespace llvm;

namespace {

class SimplifySpecialFunctions : public FunctionPass {
    static char ID;
public:

    SimplifySpecialFunctions() : FunctionPass(ID) {}

    /// for a specific (function;variableName) pair, returns a unique global variable.
    static GlobalVariable* getOrInsertUniqueGlobal(Module& m,
                                            Function* functionDecl,
                                            Type* type,
                                            StringRef variableName) {
        auto fullVariableName = ("gazer.global." + functionDecl->getName() + "." + variableName).str();
        auto var = m.getOrInsertGlobal(fullVariableName, type,
                            [&fullVariableName, &m, &type]() -> GlobalVariable* {
                        return new GlobalVariable(m, type, false,
                                GlobalVariable::LinkageTypes::InternalLinkage,
                                UndefValue::get(type),
                                fullVariableName);
                    });
        return static_cast<GlobalVariable*>(var);
    }

    bool fixPtr(CallInst* inst) {
        Function* f = inst->getCalledFunction();
        // has no arguments
        if (inst->getFunctionType()->getFunctionNumParams() != 0) {
            return false;
        }
        // returns a pointer
        if (!inst->getFunctionType()->getReturnType()->isPointerTy()) {
            return false;
        }
        // the result does not alias (mem region accessible only from this call)
        if (!f->returnDoesNotAlias()) {
            return false;
        }
        // the function does not access memory
        if (!f->doesNotAccessMemory()) {
            return false;
        }
        // the function is speculatable (~no side-effects)
        if (!f->isSpeculatable()) {
            return false;
        }
        auto resType = inst->getFunctionType()->getReturnType()->getPointerElementType();
        auto var = getOrInsertUniqueGlobal(*(f->getParent()), inst->getCalledFunction(), resType, "");
        inst->replaceAllUsesWith(var);
        inst->dropAllReferences();
        inst->eraseFromParent();
        return true;
    }

    /// Marks the given function's instructions with required
    /// pre- and postconditions.
    bool runOnFunction(llvm::Function& function) override {
        llvm::SmallVector<CallInst*, 20> to_process;
        for (auto& inst : instructions(function)) {
            if (auto* callInst = dyn_cast<CallInst>(&inst)) {
                to_process.push_back(callInst);
            }
        }
        bool changed = false;
        for (CallInst* callInst : to_process) {
            if (fixPtr(callInst)) {
                changed = true;
                continue;
            }
        }
        return changed;
    }

    void getAnalysisUsage(AnalysisUsage &Info) const override {
        Info.addRequiredID(getAnnotateSpecialFunctionsID());
    }
};

char SimplifySpecialFunctions::ID;

}

llvm::Pass* gazer::createSimplifySpecialFunctionsPass() {
    return new SimplifySpecialFunctions();
}
