//==-              - Translate LLVM IR to expressions -----------*- C++ -*--==//
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

/**
 * Every function in pthread_create is called in newly created threads with arg null...
 * pthread_create(&thrd, null, function, arg);
 */
#include <gazer/LLVM/Transform/Passes.h>

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Support/Casting.h>

/// Normalizes calls related to creating and joining threads
class RegisterThreadingPass : public llvm::ModulePass {
    static char ID;

public:
    // might contain duplicates
    std::vector<llvm::Function*> threadFunctions;
    RegisterThreadingPass() : ModulePass(ID) {}
private:
    static llvm::Function* getMainFunction(llvm::Module& m) {
        for (auto& function : m) {
            // TODO
            if (function.hasName() && function.getName()=="main") {
                return &function;
            }
        }
        return nullptr;
    }

    static llvm::Function* getOrInsertAssumeFunction(llvm::IRBuilder<>& irbuilder,
                                                llvm::Module& m) {
        if (auto *res = m.getFunction("llvm.assume")) {
            return res;
        }
        auto* type = llvm::FunctionType::get(irbuilder.getVoidTy(), {irbuilder.getInt1Ty()},false);
        return llvm::Function::Create(type, llvm::Function::InternalLinkage, "llvm.assume");
    }
    /**
     * assume(v); (void)func(nullptr);
     */
    static llvm::Function* createThreadFunction(llvm::IRBuilder<>& parentIRBuilder,
                                                llvm::GlobalValue* flag,
                                                llvm::Function* functionToCall,
                                                llvm::Module& m) {

        auto* funcType = llvm::FunctionType::get(parentIRBuilder.getVoidTy(), false);
        auto* func= llvm::Function::Create(funcType, llvm::GlobalValue::InternalLinkage, 0);
        auto* bb = llvm::BasicBlock::Create(m.getContext(), "entry", func);
        llvm::IRBuilder<> irbuilder(bb);
        irbuilder.CreateCall(getOrInsertAssumeFunction(irbuilder, m), {flag});
        irbuilder.CreateCall(functionToCall);
        irbuilder.CreateRetVoid();
        return func;
    }

    static llvm::GlobalVariable* createThreadStateVariable(llvm::IRBuilder<>& irbuilder, llvm::Module& m) {
        static std::string name = "threading.global";

        /**
         *   GlobalVariable(Module &M, Type *Ty, bool isConstant,
                 LinkageTypes Linkage, Constant *Initializer,
                 const Twine &Name = "", GlobalVariable *InsertBefore = nullptr,
                 ThreadLocalMode = NotThreadLocal, unsigned AddressSpace = 0,
                 bool isExternallyInitialized = false);
         */
        name += "_";
        auto* falseVal = irbuilder.getInt1(false);
        return new llvm::GlobalVariable(m, falseVal->getType(), false, llvm::GlobalValue::InternalLinkage, falseVal);
    }

public:
    bool runOnModule(llvm::Module& m) override {
        llvm::SmallVector<llvm::CallInst*, 4> toProcess;
        if (auto* mainFunc = getMainFunction(m)) {
            for (llvm::inst_iterator I = llvm::inst_begin(mainFunc),
                                     E = llvm::inst_end(mainFunc); I != E; ++I) {
                auto& inst = *I;
                if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                    if (callInst->getCalledFunction()->getName() == "pthread_create") {
                        toProcess.push_back(callInst);
                        if (auto* threadFunction =
                                llvm::dyn_cast<llvm::Function>(callInst->getArgOperand(2))) {
                            threadFunctions.push_back(threadFunction);
                        } else {
                            llvm_unreachable("Bad pthread_create call");
                        }
                    }
                }
            }
        }
        bool changed = false;
        for (auto* proc : toProcess) {
            llvm::IRBuilder<> irbuilder(m.getContext());
            //irbuilder.get
            auto* stateVar = createThreadStateVariable(irbuilder, m);
            irbuilder.CreateStore(irbuilder.getInt1(true), stateVar);
            if (auto* threadFunction =
                llvm::dyn_cast<llvm::Function>(proc->getArgOperand(2))) {
                createThreadFunction(irbuilder, stateVar, threadFunction, m);
            } else {
                llvm_unreachable("Bad pthread_create call");
            }
            //proc->replaceAllUsesWith(???);
            /*for (auto& use : thread.thrd_id->uses()) {
                if (auto* loadInst = llvm::dyn_cast<llvm::LoadInst>(use.getUser())) {
                    for (auto& loadUse : loadInst->uses()) {
                        if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(loadUse.getUser())) {
                            if (callInst->getCalledFunction()->getName() == "pthread_join") {
                                assert(
                                    callInst->getArgOperand(1) == loadUse
                                    && "pthread_join wrong use");
                                changed = true;
                                llvm::IRBuilder<> builder(m.getContext());
                                builder.ClearInsertionPoint();
                                callInst->replaceAllUsesWith(builder.CreateCall(getJoinThreadCallFor(m, thread)));
                                callInst->dropAllReferences();
                            } else {
                                llvm_unreachable("Unknown use of thread info");
                            }
                        } else {
                            llvm_unreachable("Unknown use of thread info");
                        }
                    }
                } else {
                    if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(use.getUser())) {
                        if (callInst->getFunction()->getName() != "pthread_create") {
                            llvm_unreachable("Unkown use of thread info");
                        }
                        //changed = true;
                    } else {
                        llvm_unreachable("Unknown use of thread info");
                    }
                }
            }*/
        }
        return changed;
        //thrd_id->uses();
    }
};

llvm::Pass* gazer::createNormalizeThreadingCallsPass() {
    return new RegisterThreadingPass();
}
