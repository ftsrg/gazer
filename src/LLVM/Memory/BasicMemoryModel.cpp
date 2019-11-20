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
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/Core/ExprTypes.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/InstIterator.h>

using namespace gazer;
using namespace gazer::memory;

namespace
{

class BasicMemoryModel : public MemoryModel
{
    struct FunctionMemoryInfo
    {
        llvm::DenseMap<llvm::Value*, MemoryObject*> Objects;
        llvm::DenseMap<MemoryObject*, llvm::Value*> ObjectToValue;
        llvm::DenseMap<MemoryObject*, memory::LiveOnEntryDef*> EntryDefs;

        void addObject(llvm::Value* source, MemoryObject* object)
        {
            Objects[source] = object;
            ObjectToValue[object] = source;
        }
    };
public:
    using MemoryModel::MemoryModel;

    ExprPtr handleAlloca(
        const llvm::AllocaInst& alloca,
        const llvm::SmallVectorImpl<memory::AllocaDef*>& annotations
    ) override;

    ExprPtr handleLoad(
        const llvm::LoadInst& load,
        const llvm::SmallVectorImpl<memory::LoadUse*>& annotations,
        ExprPtr pointer,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override;

    void handleCall(
        llvm::CallSite call,
        const llvm::SmallVectorImpl<memory::CallUse*>& useAnnotations,
        const llvm::SmallVectorImpl<memory::CallDef*>& defAnnotations,
        llvm::SmallVectorImpl<CallParam>& callParams
    ) override;

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
        const llvm::SmallVectorImpl<memory::StoreDef*>& annotations,
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
    MemoryObject* trackPointerToMemoryObject(FunctionMemoryInfo& function, llvm::Value* value);

    ExprPtr ptrForMemoryObject(MemoryObject* object) {
        return IntLiteralExpr::Get(IntType::Get(mContext), object->getId());
    }

private:
    llvm::DenseMap<llvm::Function*, FunctionMemoryInfo> mFunctions;
    unsigned mId = 0;
};

} // end anonymous namespace

MemoryObject* BasicMemoryModel::trackPointerToMemoryObject(FunctionMemoryInfo& function, llvm::Value* value)
{
    assert(value->getType()->isPointerTy());

    llvm::Value* ptr = value;
    while (true) {
        MemoryObject* object = function.Objects.lookup(ptr);
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
    auto& currentObjects = mFunctions[&function];
    unsigned tmp = 0;

    // TODO: This should be more flexible.
    bool isEntryFunction = function.getName() == "main";
    llvm::SmallPtrSet<llvm::Value*, 32> allocSites;

    // Each function will have a memory object made from this global variable.
    for (llvm::GlobalVariable& gv : function.getParent()->globals()) {
        auto gvTy = gv.getType()->getPointerElementType();
        auto object = builder.createMemoryObject(
            mId++,
            MemoryObjectType::Scalar,
            getDataLayout().getTypeAllocSize(gvTy),
            gvTy,
            gv.getName()
        );

        if (isEntryFunction) {
            builder.createGlobalInitializerDef(object, gv.hasInitializer() ? gv.getInitializer() : nullptr);
        } else {
            currentObjects.EntryDefs[object] = builder.createLiveOnEntryDef(object);
        }
        currentObjects.addObject(&gv, object);
        allocSites.insert(&gv);
    }

    for (llvm::Argument& arg : function.args()) {
        llvm::Type* argTy = arg.getType();
        if (argTy->isPointerTy()) {
            std::string name = arg.hasName() ? arg.getName().str() : ("arg_" + std::to_string(tmp++));

            auto object = builder.createMemoryObject(
                mId++,
                MemoryObjectType::Scalar,
                getDataLayout().getTypeAllocSize(argTy->getPointerElementType()),
                argTy->getPointerElementType(),
                name
            );

            currentObjects.EntryDefs[object] = builder.createLiveOnEntryDef(object);
            currentObjects.addObject(&arg, object);
            allocSites.insert(&arg);
        }
    }

    // Add all alloca instructions
    for (llvm::Instruction& inst : llvm::instructions(function)) {
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(&inst)) {
            llvm::Type* allocatedTy = alloca->getType()->getPointerElementType();
            std::string name = alloca->hasName() ? alloca->getName().str() : ("alloca_" + std::to_string(tmp++));
            auto object = builder.createMemoryObject(
                mId++,
                MemoryObjectType::Scalar,
                getDataLayout().getTypeAllocSize(allocatedTy),
                allocatedTy,
                name
            );

            builder.createAllocaDef(object, *alloca);
            currentObjects.addObject(alloca, object);
            allocSites.insert(alloca);
        }
    }

    // Now, walk over all instructions and search for pointer operands
    for (llvm::Instruction& inst : llvm::instructions(function)) {
        if (auto store = llvm::dyn_cast<llvm::StoreInst>(&inst)) {
            MemoryObject* object = trackPointerToMemoryObject(currentObjects, store->getPointerOperand());
            if (object == nullptr) {
                // We could not track this pointer to an origin,
                // we must clobber all memory objects in order to be safe.
                for (auto& entry : currentObjects.Objects) {
                    builder.createStoreDef(entry.second, *store);
                }
            } else {
                // Otherwise, just create a definition for this one object.
                builder.createStoreDef(object, *store);
            }
        } else if (auto load = llvm::dyn_cast<llvm::LoadInst>(&inst)) {
            MemoryObject* object = trackPointerToMemoryObject(currentObjects, load->getPointerOperand());
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
                    MemoryObject* object = trackPointerToMemoryObject(currentObjects, arg);
                    if (object == nullptr) {
                        // The memory object could not be resolved, we have to clobber all of them.
                        for (auto& entry : currentObjects.Objects) {
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
        } else if (auto ret = llvm::dyn_cast<llvm::ReturnInst>(&inst)) {
            // A use annotation on a return instruction indicates that the memory object
            // should be considered alive after the function returns.
            // In this memory model, all memory objects except the ones allocated with
            // an alloca will be considered alive on return.
            for (auto& [allocation, object] : currentObjects.Objects) {
                if (!llvm::isa<llvm::AllocaInst>(allocation)) {
                    builder.createReturnUse(object, *ret);
                }
            }
        }
    }
}

void BasicMemoryModel::handleCall(
    llvm::CallSite call,
    const llvm::SmallVectorImpl<memory::CallUse*>& useAnnotations,
    const llvm::SmallVectorImpl<memory::CallDef*>& defAnnotations,
    llvm::SmallVectorImpl<CallParam>& callParams
)
{
    assert(call.getCalledFunction() != nullptr);

    FunctionMemoryInfo& callerInfo = mFunctions[call.getParent()->getParent()];
    FunctionMemoryInfo& calleeInfo = mFunctions[call.getCalledFunction()];

    for (memory::CallUse* use : useAnnotations) {
        llvm::Value* valueInCaller = callerInfo.ObjectToValue[use->getObject()];
    }
}

void BasicMemoryModel::declareProcedureVariables(llvm2cfa::VariableDeclExtensionPoint& extensionPoint)
{
}

void BasicMemoryModel::handleStore(
    const llvm::StoreInst& store,
    const llvm::SmallVectorImpl<memory::StoreDef*>& annotations,
    ExprPtr pointer, ExprPtr value, llvm2cfa::GenerationStepExtensionPoint& ep
) {
    if (annotations.size() == 0) {
        // If this store has no annotations, do nothing.
        return;
    }

    if (annotations.size() == 1) {
        Variable* defVariable = ep.getVariableFor(annotations[0]);
        assert(defVariable != nullptr && "Each memory object definition should map to a variable in the CFA!");
        assert(defVariable->getType() == value->getType());

        ep.insertAssignment({defVariable, value});
        return;
    }

    // If a single store may clobber multiple memory objects, we disambiguate at 'runtime',
    // using the pointer values.
    for (memory::StoreDef* def : annotations) {
        Variable* defVariable = ep.getVariableFor(def);
        assert(defVariable != nullptr && "Each memory object definition should map to a variable in the CFA!");
        assert(defVariable->getType() == value->getType());

        MemoryObjectDef* reachingDef = def->getReachingDef();
        assert(reachingDef != nullptr && "Store without a previous reaching definition?");

        auto select = SelectExpr::Create(
            EqExpr::Create(pointer, ptrForMemoryObject(def->getObject())),
            value,
            ep.operand(reachingDef)
        );

        ep.insertAssignment({defVariable, select});
    }
}

ExprPtr BasicMemoryModel::handleLoad(
    const llvm::LoadInst& load,
    const llvm::SmallVectorImpl<memory::LoadUse*>& annotations,
    ExprPtr pointer,
    llvm2cfa::GenerationStepExtensionPoint& ep
) {
    if (annotations.size() == 0) {
        // If no memory access annotations are available for this load,
        // then the memory model was unable to resolve it.
        // We will over-approximate, and return an undef expression.
        return UndefExpr::Get(translateType(load.getType()));
    }

    if (annotations.size() == 1) {
        MemoryObjectUse* use = annotations[0];
        MemoryObjectDef* def = use->getReachingDef();
        assert(def != nullptr && "There must be a reaching definition for this load!");

        return ep.operand(def);
    }

    // If this load may be clobbered by multiple definitions, we disambiguate using the pointer values.
    ExprPtr expr = UndefExpr::Get(translateType(load.getType()));

    for (memory::LoadUse* use : annotations) {
        MemoryObjectDef* reachingDef = use->getReachingDef();
        Variable* defVariable = ep.getVariableFor(reachingDef);
        assert(defVariable != nullptr && "Each memory object definition should map to a variable in the CFA!");

        expr = SelectExpr::Create(
            EqExpr::Create(pointer, ptrForMemoryObject(reachingDef->getObject())),
            ep.operand(reachingDef),
            expr
        );
    }

    return expr;
}

void BasicMemoryModel::handleBlock(const llvm::BasicBlock& bb, llvm2cfa::GenerationStepExtensionPoint& ep)
{
    /*
    auto& memSSA = getFunctionMemorySSA(*bb.getParent());
    for (MemoryObjectDef& def : memSSA.definitionAnnotationsFor(&bb)) {
        Variable* defVariable = ep.getVariableFor(&def);
        assert(defVariable != nullptr && "Each memory object definition should map to a variable in the CFA!");
        if (auto globalInit = llvm::dyn_cast<GlobalInitializerDef>(&def)) {
            ep.insertAssignment({defVariable, ep.operand(globalInit->getInitializer())});
        }
    } */
}

ExprPtr BasicMemoryModel::handleAlloca(
    const llvm::AllocaInst& alloca, const llvm::SmallVectorImpl<memory::AllocaDef*>& annotations
) {
    assert(annotations.size() && "An alloca inst must define exactly one memory object!");

    MemoryObject* object = annotations[0]->getObject();
    return ptrForMemoryObject(object);
}

auto gazer::CreateBasicMemoryModel(
    GazerContext& context,
    const LLVMFrontendSettings& settings,
    const llvm::DataLayout& dl
) -> std::unique_ptr<MemoryModel>
{
    return std::make_unique<BasicMemoryModel>(context, settings, dl);
}
