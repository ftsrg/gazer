//==- InstToExpr.h - Translate LLVM IR to expressions -----------*- C++ -*--==//
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
 * thrd_t -> struct Func { int current_thrd_id; void* arg; void* result; };
 *
 * pthread_t thrd;
 * pthread_create(&thrd, null, function, arg);
 * pthread_join(&thrd, &result);
 */
#include <gazer/LLVM/Transform/Passes.h>

#include <llvm/Pass.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Support/Casting.h>

/// Normalizes calls related to creating and joining threads
class NormalizeThreadingPass : public llvm::ModulePass {
    static char ID;

    struct ThreadInfo {
        llvm::Value* thrd_id;
        llvm::Function* calledFunction;
        llvm::CallInst* createThreadCall;
        ThreadInfo(llvm::Value* t, llvm::Function* f, llvm::CallInst* ctc) : thrd_id(t), calledFunction(f), createThreadCall(ctc) {}
        ThreadInfo(const ThreadInfo&) = delete;
        ThreadInfo(ThreadInfo&&) = delete;
    };

public:
    NormalizeThreadingPass() : ModulePass(ID) {}
    static llvm::Function* getMainFunction(llvm::Module& m) {
        for (auto& function : m) {
            // TODO
            if (function.hasName() && function.getName()=="main") {
                return &function;
            }
        }
        return nullptr;
    }

    llvm::SmallDenseMap<ThreadInfo*, int, 4> threadIds;
    llvm::FunctionCallee getJoinThreadCallFor(llvm::Module& m, ThreadInfo& info) {
        int threadId = threadIds.lookup(&info);
        const auto& name = "gazer.xcfa.jointhread." + llvm::Twine(threadId);
        auto boolType = llvm::FunctionType::get(llvm::IntegerType::get(m.getContext(), 1), false);
        return m.getOrInsertFunction(name.str(), boolType);
    }

    int ctr=0;
    llvm::FunctionCallee getCreateThreadCallFor(llvm::Module& m, ThreadInfo& info) {
        int threadId = ++ctr;
        threadIds[&info] = threadId;
        const auto& name = "gazer.xcfa.createthread." + llvm::Twine(threadId);
        auto* type = llvm::FunctionType::get(llvm::IntegerType::get(m.getContext(), 1), false);
        return m.getOrInsertFunction(name.str(), type);
    }

    bool runOnModule(llvm::Module& m) override {
        llvm::SmallVector<ThreadInfo, 4> threads;
        if (auto* mainFunc = getMainFunction(m)) {
            for (llvm::inst_iterator I = llvm::inst_begin(mainFunc),
                                     E = llvm::inst_end(mainFunc); I != E; ++I) {
                auto& inst = *I;
                if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                    if (callInst->getCalledFunction()->getName() == "pthread_create") {
                        if (inst.getParent() == &mainFunc->getEntryBlock()) {
                            llvm_unreachable("pthread_create called outside main's entry block");
                        }
                        auto* thrd_id = llvm::dyn_cast<llvm::Value>(callInst->getArgOperand(0));
                        if (thrd_id == nullptr) {
                            llvm_unreachable("");
                        }
                        auto* func = callInst->getArgOperand(2);
                        if (auto* glob = llvm::dyn_cast<llvm::Function>(func)) {
                            threads.emplace_back(thrd_id, glob, callInst);
                        } else {
                            llvm_unreachable("Invalid pthread_create function");
                        }
                    }
                }
            }
        }
        bool changed = false;
        for (ThreadInfo& thread : threads) {
            for (auto& use : thread.thrd_id->uses()) {
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
            }
        }
        return changed;
        //thrd_id->uses();
    }
};

llvm::Pass* gazer::createNormalizeThreadingCallsPass() {
    return new NormalizeThreadingPass();
}
