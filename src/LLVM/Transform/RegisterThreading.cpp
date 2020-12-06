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

namespace {

/// Normalizes calls related to creating and joining threads
class RegisterThreadingPass : public llvm::ModulePass
{
    static char ID;

    // might contain duplicates
    std::vector<llvm::Function*> threadFunctions;
    gazer::LLVMFrontendSettings& settings;

public:
    RegisterThreadingPass(gazer::LLVMFrontendSettings& settings)
        : ModulePass(ID), settings(settings)
    {}

private:
    llvm::Function* getMainFunction(llvm::Module& m)
    {
        return settings.getEntryFunction(m);
    }

    static llvm::Function* getOrInsertAssumeFunction(llvm::IRBuilder<>& irbuilder, llvm::Module& m)
    {
        if (auto* res = m.getFunction("llvm.assume")) {
            return res;
        }
        auto* type = llvm::FunctionType::get(irbuilder.getVoidTy(), {irbuilder.getInt1Ty()}, false);
        return llvm::Function::Create(type, llvm::Function::ExternalLinkage, "llvm.assume", m);
    }


    /**
     * assume(v); (void)func(nullptr);
     */
    static llvm::Function* createThreadFunction(
        llvm::IRBuilder<>& parentIRBuilder,
        llvm::GlobalValue* flag,
        llvm::Function* functionToCall,
        llvm::Module& m)
    {
        static int ctr = 0;
        auto* funcType = llvm::FunctionType::get(parentIRBuilder.getVoidTy(), false);
        auto* func = llvm::Function::Create(
            funcType, llvm::GlobalValue::ExternalLinkage, "__ThreadFunction" + llvm::Twine(++ctr), m);
        auto* bb = llvm::BasicBlock::Create(m.getContext(), "entry", func);
        llvm::IRBuilder<> irbuilder(bb);
        auto* flagLoad = irbuilder.CreateLoad(flag);
        irbuilder.CreateCall(getOrInsertAssumeFunction(irbuilder, m), {flagLoad});

        auto* nullVal =
            llvm::Constant::getNullValue(functionToCall->getFunctionType()->getParamType(0));
        llvm::errs() << functionToCall->getFunctionType();
        // auto* nullPtr = llvm::ConstantPointerNull::get(llvm::PointerType::get(irbuilder.getVoidTy(),0));
        irbuilder.CreateCall(functionToCall, {nullVal});
        irbuilder.CreateRetVoid();
        return func;
    }

    static llvm::GlobalVariable*
        createThreadStateVariable(llvm::IRBuilder<>& irbuilder, llvm::Module& m)
    {
        static int ctr = 0;
        /**
         *   GlobalVariable(Module &M, Type *Ty, bool isConstant,
                 LinkageTypes Linkage, Constant *Initializer,
                 const Twine &Name = "", GlobalVariable *InsertBefore = nullptr,
                 ThreadLocalMode = NotThreadLocal, unsigned AddressSpace = 0,
                 bool isExternallyInitialized = false);
         */
        auto* falseVal = irbuilder.getInt1(false);
        return new llvm::GlobalVariable(
            m, falseVal->getType(), false, llvm::GlobalValue::ExternalLinkage, falseVal, "threading_global" + llvm::Twine(++ctr));
    }

public:
    bool runOnModule(llvm::Module& m) override
    {
        llvm::SmallVector<llvm::CallInst*, 4> toProcess;
        if (auto* mainFunc = getMainFunction(m)) {
            for (llvm::inst_iterator I = llvm::inst_begin(mainFunc), E = llvm::inst_end(mainFunc);
                 I != E; ++I) {
                auto& inst = *I;
                if (auto* callInst = llvm::dyn_cast<llvm::CallInst>(&inst)) {
                    if (callInst->getCalledFunction()->getName() == "pthread_create") {
                        toProcess.push_back(callInst);
                    }
                }
            }
        }
        bool changed = false;
        for (auto* proc : toProcess) {
            changed = true;
            llvm::IRBuilder<> irbuilder(proc);
            // irbuilder.get
            auto* stateVar = createThreadStateVariable(irbuilder, m);
            irbuilder.CreateStore(irbuilder.getInt1(true), stateVar);
            if (auto* threadFunction = llvm::dyn_cast<llvm::Function>(proc->getArgOperand(2))) {
                auto* threadMain = createThreadFunction(irbuilder, stateVar, threadFunction, m);
                settings.registerThread(threadMain);
            } else {
                llvm_unreachable("Bad pthread_create call");
            }
            proc->replaceAllUsesWith(llvm::Constant::getNullValue(proc->getFunctionType()->getReturnType()));
            proc->dropAllReferences();
            proc->eraseFromParent();
        }
        return changed;
    }
};

char RegisterThreadingPass::ID;

} // namespace

llvm::Pass* gazer::createRegisterThreadingPass(gazer::LLVMFrontendSettings& settings) {
    return new RegisterThreadingPass(settings);
}
