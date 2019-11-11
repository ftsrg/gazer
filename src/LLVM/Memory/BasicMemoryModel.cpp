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

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

using namespace gazer;
using namespace gazer::memory;

namespace
{

class BasicMemoryModel : public MemoryModel
{
public:
    using MemoryModel::MemoryModel;

    ExprPtr handleLoad(const llvm::LoadInst& load) override
    {
        return UndefExpr::Get(mTypes.get(load.getType()));
    }

    ExprPtr handleGetElementPtr(const llvm::GEPOperator& gep) override
    {
        return UndefExpr::Get(IntType::Get(mContext));
    }

    ExprPtr handleAlloca(const llvm::AllocaInst& alloc) override
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

    std::optional<VariableAssignment> handleStore(const llvm::StoreInst& store, ExprPtr pointer, ExprPtr value) override
    {
        return std::nullopt;
    }

    Type& handlePointerType(const llvm::PointerType* type) override
    {
        return IntType::Get(mContext);
    }

    Type& handleArrayType(const llvm::ArrayType* type) override
    {
        return IntType::Get(mContext);
    }

    void declareProcedureVariables(Cfa& cfa, llvm::Function& function) override;

    void declareProcedureVariables(Cfa& cfa, llvm::Loop* loop) override;

protected:
    void initializeFunction(llvm::Function& function, MemorySSABuilder& builder) override;

private:
    llvm::DenseMap<llvm::Value*, MemoryObject*> mObjects;
};

} // end anonymous namespace

void BasicMemoryModel::initializeFunction(llvm::Function& function, MemorySSABuilder& builder)
{
    // Each function will have a memory object made from this global variable.
    for (llvm::GlobalVariable& gv : function.getParent()->globals()) {
        auto gvTy = gv.getType()->getPointerElementType();
        auto object = builder.createMemoryObject(
            MemoryObjectType::Scalar,
            getDataLayout().getTypeAllocSize(gvTy),
            gvTy,
            gv.getName()
        );

        builder.createLiveOnEntry(object);
        mObjects[&gv] = object;
    }

    // Walk over all uses of these global variables
    for (llvm::GlobalVariable& gv : function.getParent()->globals()) {
        MemoryObject* object = mObjects[&gv];
        for (auto& use : gv.uses()) {
            if (auto store = llvm::dyn_cast<llvm::StoreInst>(use.getUser())) {
                if (store->getPointerOperand() == &gv) {
                    builder.createStoreDef(object, *store);
                }
            } else if (auto load = llvm::dyn_cast<llvm::LoadInst>(use.getUser())) {
                builder.createLoadUse(object, *load);
            }
        }
    }
}

static std::string getMemoryObjectName(MemoryObjectDef& def)
{
    MemoryObject* object = def.getObject();
    if (!object->getName().empty()) {
        return (object->getName() + "_" + llvm::Twine(def.getVersion())).str();
    }

    return ("__mem_object_" + llvm::Twine(object->getId()) + "_" + llvm::Twine(def.getVersion())).str();
}

void BasicMemoryModel::declareProcedureVariables(Cfa& cfa, llvm::Function& function)
{
    auto& functionMemoryModel = getFunctionMemorySSA(function);
    for (MemoryObject& object : functionMemoryModel.objects()) {
        for (MemoryObjectDef& def : object.defs()) {
            if (auto liveOnEntry = llvm::dyn_cast<memory::LiveOnEntryDef>(&def)) {
                cfa.createInput(
                    getMemoryObjectName(def),
                    translateType(object.getValueType())
                );
            } else {
                cfa.createLocal(
                    getMemoryObjectName(def),
                    translateType(object.getValueType())
                );
            }
        }
    }
}

void BasicMemoryModel::declareProcedureVariables(Cfa& cfa, llvm::Loop* loop)
{

}

auto gazer::CreateBasicMemoryModel(
    GazerContext& context,
    const LLVMFrontendSettings& settings,
    const llvm::DataLayout& dl
) -> std::unique_ptr<MemoryModel>
{
    return std::make_unique<BasicMemoryModel>(context, settings, dl);
}
