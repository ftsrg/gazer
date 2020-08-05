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
#include "gazer/LLVM/Instrumentation/Intrinsics.h"
#include "gazer/LLVM/Transform/Passes.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IntrinsicInst.h>

using namespace gazer;

namespace
{

class TrackLocalVariableWriteLocationsPass : public llvm::ModulePass
{
public:
    static char ID;

    TrackLocalVariableWriteLocationsPass()
        : ModulePass(ID)
    {}

    bool runOnModule(llvm::Module& module) override;
    bool runOnFunction(llvm::Function& function);
};

} // namespace

char TrackLocalVariableWriteLocationsPass::ID;

bool TrackLocalVariableWriteLocationsPass::runOnModule(llvm::Module& module)
{
    bool modified = false;
    for (llvm::Function& function : module) {
        if (function.isDeclaration()) {
            continue;
        }

        modified |= runOnFunction(function);
    }

    return modified;
}

static bool handleDbgDeclare(llvm::DbgDeclareInst* declare) {
    llvm::Module* module = declare->getModule();
    llvm::Metadata* variable = declare->getRawVariable();
    llvm::Value* address = declare->getVariableLocation();

    if (address == nullptr || !llvm::isa<llvm::DILocalVariable>(variable)) {
        return false;
    }

    // Find all stores to this address and add an assignment for it
    for (llvm::Use& use : address->uses()) {
        llvm::User* user = use.getUser();
        if (!llvm::isa<llvm::StoreInst>(user) || use.getOperandNo() != 1) {
            continue;
        }

        auto store = llvm::cast<llvm::StoreInst>(user);
        auto storedValue = store->getValueOperand();

        auto write = GazerIntrinsic::GetOrInsertLocalVariableWrite(*module, storedValue->getType());

        auto call = llvm::CallInst::Create(
            write.getFunctionType(), write.getCallee(),
            { storedValue, llvm::MetadataAsValue::get(module->getContext(), variable) },
            "",
            store
        );
        call->setDebugLoc(store->getDebugLoc());
    }

    return address->getNumUses() != 0;
}

bool TrackLocalVariableWriteLocationsPass::runOnFunction(llvm::Function &function)
{
    bool modified = false;
    for (llvm::Instruction& inst : llvm::instructions(function)) {
        if (auto declare = llvm::dyn_cast<llvm::DbgDeclareInst>(&inst)) {
            modified |= handleDbgDeclare(declare);
        }
    }

    return modified;
}

llvm::Pass* gazer::createTrackLocalVariableWriteLocationsPass()
{
    return new TrackLocalVariableWriteLocationsPass();
}


