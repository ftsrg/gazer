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
#include "gazer/LLVM/Memory/MemoryModel.h"
#include "gazer/LLVM/Memory/MemorySSA.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Automaton/Cfa.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>

using namespace gazer;
using namespace gazer::memory;

namespace
{

class BasicMemoryModel : public MemoryModel
{
public:
    using MemoryModel::MemoryModel;

    ExprPtr handleAlloca(const llvm::AllocaInst& alloca) override;
    ExprPtr handleLoad(const llvm::LoadInst& load, llvm2cfa::GenerationStepExtensionPoint& ep) override;

    void handleBlock(const llvm::BasicBlock& bb, llvm2cfa::GenerationStepExtensionPoint& ep) override;

    ExprPtr handleGetElementPtr(const llvm::GEPOperator& gep) override
    {
        return UndefExpr::Get(IntType::Get(mContext));
    }

    ExprPtr handlePointerCast(const llvm::CastInst& cast) override
    {
        return UndefExpr::Get(IntType::Get(mContext));
    }

    ExprPtr handlePointerValue(const llvm::Value* value) override
    {
        return UndefExpr::Get(IntType::Get(mContext));
    }

    void handleStore(
        const llvm::StoreInst& store,
        ExprPtr pointer,
        ExprPtr value,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override;

    Type& handlePointerType(const llvm::PointerType* type) override
    {
        return IntType::Get(mContext);
    }

    Type& handleArrayType(const llvm::ArrayType* type) override
    {
        return IntType::Get(mContext);
    }

    void declareProcedureVariables(llvm2cfa::VariableDeclExtensionPoint& extensionPoint) override;

protected:
    void initializeFunction(llvm::Function& function, MemorySSABuilder& builder) override;

private:
    MemoryObject* trackPointerToMemoryObject(llvm::Value* value);

private:
    llvm::DenseMap<llvm::Value*, MemoryObject*> mObjects;
};

} // end anonymous namespace

MemoryObject* BasicMemoryModel::trackPointerToMemoryObject(llvm::Value* value)
{
    assert(value->getType()->isPointerTy());

    llvm::Value* ptr = value;
    while (true) {
        MemoryObject* object = mObjects.lookup(ptr);
        if (object != nullptr) {
            return object;
        }

        if (auto bitcast = llvm::dyn_cast<llvm::BitCastInst>(ptr)) {
            ptr = bitcast->getOperand(0);
        }

        // We cannot track this pointer any further.
        break;
    }

    return nullptr;
}

void BasicMemoryModel::initializeFunction(llvm::Function& function, MemorySSABuilder& builder)
{
    mObjects.clear();
    unsigned tmp = 0;

    // TODO: This should be more flexible.
    bool isEntryFunction = function.getName() == "main";
    llvm::SmallPtrSet<llvm::Value*, 32> allocSites;

    // Each function will have a memory object made from this global variable.
    for (llvm::GlobalVariable& gv : function.getParent()->globals()) {
        auto gvTy = gv.getType()->getPointerElementType();
        auto object = builder.createMemoryObject(
            MemoryObjectType::Scalar,
            getDataLayout().getTypeAllocSize(gvTy),
            gvTy,
            gv.getName()
        );

        if (isEntryFunction) {
            builder.createGlobalInitializerDef(object, gv.hasInitializer() ? gv.getInitializer() : nullptr);
        } else {
            builder.createLiveOnEntryDef(object);
        }
        mObjects[&gv] = object;
        allocSites.insert(&gv);
    }

    for (llvm::Argument& arg : function.args()) {
        llvm::Type* argTy = arg.getType();
        if (argTy->isPointerTy()) {
            std::string name = arg.hasName() ? arg.getName().str() : ("arg_" + std::to_string(tmp++));

            auto object = builder.createMemoryObject(
                MemoryObjectType::Scalar,
                getDataLayout().getTypeAllocSize(argTy->getPointerElementType()),
                argTy->getPointerElementType(),
                name
            );

            builder.createLiveOnEntryDef(object);
            mObjects[&arg] = object;
            allocSites.insert(&arg);
        }
    }

    // Add all alloca instructions
    for (llvm::Instruction& inst : llvm::instructions(function)) {
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(&inst)) {
            llvm::Type* allocatedTy = alloca->getType()->getPointerElementType();
            std::string name = alloca->hasName() ? alloca->getName().str() : ("alloca_" + std::to_string(tmp++));
            auto object = builder.createMemoryObject(
                MemoryObjectType::Scalar,
                getDataLayout().getTypeAllocSize(allocatedTy),
                allocatedTy,
                name
            );

            builder.createAllocaDef(object, *alloca);
            mObjects[alloca] = object;
            allocSites.insert(alloca);
        }
    }

    // Now, walk over all instructions and search for pointer operands
    for (llvm::Instruction& inst : llvm::instructions(function)) {
        if (auto store = llvm::dyn_cast<llvm::StoreInst>(&inst)) {
            MemoryObject* object = trackPointerToMemoryObject(store->getPointerOperand());
            if (object == nullptr) {
                // We could not track this pointer to an origin,
                // we must clobber all memory objects in order to be safe.
                for (auto& entry : mObjects) {
                    builder.createStoreDef(entry.second, *store);
                }
            } else {
                // Otherwise, just create a definition for this one object.
                builder.createStoreDef(object, *store);
            }
        } else if (auto load = llvm::dyn_cast<llvm::LoadInst>(&inst)) {
            MemoryObject* object = trackPointerToMemoryObject(load->getPointerOperand());
            // If the object is nullptr, we could not track the origins of the pointer.
            // We will not insert any annotations, and the LoadInst will be translated to
            // an undef value.
            if (object != nullptr) {
                builder.createLoadUse(object, *load);
            }
        } else if (auto call = llvm::dyn_cast<llvm::CallInst>(&inst)) {
            llvm::Function* callee = call->getCaller();
            llvm::SmallPtrSet<MemoryObject*, 8> definedObjects;
            llvm::SmallPtrSet<MemoryObject*, 8> usedObjects;

            for (unsigned i = 0; i < call->getNumArgOperands(); ++i) {
                llvm::Value* arg = call->getArgOperand(i);
                if (arg->getType()->isPointerTy()) {
                    MemoryObject* object = trackPointerToMemoryObject(arg);
                    if (object == nullptr) {
                        for (auto& entry : mObjects) {
                            definedObjects.insert(entry.second);
                            usedObjects.insert(entry.second);
                        }
                        break;
                    } else {
                        definedObjects.insert(object);
                        usedObjects.insert(object);
                    }
                }
            }

            if (auto ii = llvm::dyn_cast<llvm::IntrinsicInst>(call)) {
                // TODO
                continue;
            }

            for (MemoryObject* object : definedObjects) {
                builder.createCallDef(object, call);
            }

            for (MemoryObject* object : usedObjects) {
                builder.createCallUse(object, call);
            }
        }
    }
}

void BasicMemoryModel::declareProcedureVariables(llvm2cfa::VariableDeclExtensionPoint& extensionPoint)
{
    if (llvm::Function* function = extensionPoint.getSourceFunction()) {
        auto& functionMemoryModel = getFunctionMemorySSA(*function);
        for (MemoryObject& object : functionMemoryModel.objects()) {
            for (MemoryObjectDef& def : object.defs()) {
                if (auto liveOnEntry = llvm::dyn_cast<memory::LiveOnEntryDef>(&def)) {
                    extensionPoint.createInput(&def, translateType(object.getValueType()), "_mem");
                } else {
                    extensionPoint.createLocal(&def, translateType(object.getValueType()), "_mem");
                }
            }
        }
    }
}

void BasicMemoryModel::handleStore(
    const llvm::StoreInst& store, ExprPtr pointer, ExprPtr value, llvm2cfa::GenerationStepExtensionPoint& ep
) {
    auto& memSSA = getFunctionMemorySSA(*store.getFunction());
    for (MemoryObjectDef& def : memSSA.definitionAnnotationsFor(&store)) {
        Variable* defVariable = ep.getVariableFor(&def);
        assert(defVariable != nullptr && "Each memory object definition should map to a variable in the CFA!");
        assert(defVariable->getType() == value->getType());

        ep.insertAssignment({defVariable, value});
    }
}

ExprPtr BasicMemoryModel::handleLoad(const llvm::LoadInst& load, llvm2cfa::GenerationStepExtensionPoint& ep)
{
    auto& memSSA = getFunctionMemorySSA(*load.getFunction());
    auto range = memSSA.useAnnotationsFor(&load);

    size_t numUses = std::distance(range.begin(), range.end());
    if (numUses != 1) {
        return UndefExpr::Get(translateType(load.getType()));
    }

    MemoryObjectUse& use = *range.begin();
    MemoryObjectDef* def = use.getReachingDef();
    assert(def != nullptr && "There must be a reaching definition for this load!");

    Variable* defVariable = ep.getVariableFor(def);
    assert(defVariable != nullptr && "Each memory object definition should map to a variable in the CFA!");

    return defVariable->getRefExpr();
}

void BasicMemoryModel::handleBlock(const llvm::BasicBlock& bb, llvm2cfa::GenerationStepExtensionPoint& ep)
{
    auto& memSSA = getFunctionMemorySSA(*bb.getParent());
    for (MemoryObjectDef& def : memSSA.definitionAnnotationsFor(&bb)) {
        Variable* defVariable = ep.getVariableFor(&def);
        assert(defVariable != nullptr && "Each memory object definition should map to a variable in the CFA!");
        if (auto globalInit = llvm::dyn_cast<GlobalInitializerDef>(&def)) {
            ep.insertAssignment({defVariable, ep.operand(globalInit->getInitializer())});
        }
    }
}

ExprPtr BasicMemoryModel::handleAlloca(const llvm::AllocaInst& alloca)
{
    auto& memSSA = getFunctionMemorySSA(*alloca.getFunction());
    auto range = memSSA.definitionAnnotationsFor(&alloca);

    size_t numDefs = std::distance(range.begin(), range.end());
    assert(numDefs == 1 && "An alloca inst must define exactly one memory object!");

    MemoryObject* object = range.begin()->getObject();

    return IntLiteralExpr::Get(IntType::Get(mContext), object->getId());
}

auto gazer::CreateBasicMemoryModel(
    GazerContext& context,
    const LLVMFrontendSettings& settings,
    const llvm::DataLayout& dl
) -> std::unique_ptr<MemoryModel>
{
    return std::make_unique<BasicMemoryModel>(context, settings, dl);
}
