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

#include "gazer/Core/LiteralExpr.h"
#include "gazer/LLVM/TypeTranslator.h"

using namespace gazer;

namespace
{

class DummyMemoryModel : public MemoryModel
{
public:
    using MemoryModel::MemoryModel;

    void declareProcedureVariables(llvm2cfa::VariableDeclExtensionPoint&) override;

    ExprPtr handleGetElementPtr(
        const llvm::GetElementPtrInst& gep,
        llvm::ArrayRef<ExprPtr> ops
    ) override {
        return UndefExpr::Get(ops[0]->getType());
    }

    ExprPtr handleAlloca(
        const llvm::AllocaInst& alloc,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override {
        return UndefExpr::Get(BoolType::Get(mContext));
    }
    ExprPtr handlePointerCast(const llvm::CastInst& cast, ExprPtr opPtr) override;

    ExprPtr handlePointerValue(const llvm::Value* value, llvm::Function& parent) override {
        return UndefExpr::Get(BoolType::Get(mContext));
    }

    ExprPtr handleGlobalInitializer(
        memory::GlobalInitializerDef* def,
        ExprPtr pointer,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) {
        return UndefExpr::Get(
            translateType(def->getGlobalVariable()->getType()->getPointerElementType())
        );
    }

    void handleStore(
        const llvm::StoreInst& store,
        ExprPtr pointer,
        ExprPtr value,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override {}

    void handleCall(
        llvm::ImmutableCallSite call,
        llvm2cfa::GenerationStepExtensionPoint& callerEp,
        llvm2cfa::AutomatonInterfaceExtensionPoint& calleeEp,
        std::vector<VariableAssignment>& inputAssignments,
        std::vector<VariableAssignment>& outputAssignments,
        std::vector<VariableAssignment>& additionalAssignments
    )  override {};

    void handleBlock(const llvm::BasicBlock& bb, llvm2cfa::GenerationStepExtensionPoint& ep) override {}

    ExprPtr handleLoad(
        const llvm::LoadInst& load,
        ExprPtr pointer,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override {
        return UndefExpr::Get(translateType(load.getType()));
    }

    gazer::Type& handlePointerType(const llvm::PointerType* type) override;
    gazer::ArrayType& handleArrayType(const llvm::ArrayType* type) override
    {
        return ArrayType::Get(BoolType::Get(mContext), BvType::Get(mContext, 8));
    }

    ExprPtr handleConstantDataArray(
        const llvm::ConstantDataArray* cda,
        llvm::ArrayRef<ExprRef<LiteralExpr>> elements
    ) override
    {
        return ArrayLiteralExpr::Get(
            this->handleArrayType(cda->getType()),
            {}
        );
    }

protected:
    void initializeFunction(
        llvm::Function& function,
        memory::MemorySSABuilder& builder
    ) override;
};

} // end anonymous namespace

void DummyMemoryModel::initializeFunction(llvm::Function& function, memory::MemorySSABuilder& builder)
{
    // Intentionally empty.
}

ExprPtr DummyMemoryModel::handlePointerCast(const llvm::CastInst& cast, ExprPtr opPtr)
{
    return UndefExpr::Get(BoolType::Get(mContext));
}

gazer::Type& DummyMemoryModel::handlePointerType(const llvm::PointerType* type)
{
    return BvType::Get(mContext, 32);
}

void DummyMemoryModel::declareProcedureVariables(llvm2cfa::VariableDeclExtensionPoint& extensionPoint)
{
    // Intentionally empty.
}

auto gazer::CreateHavocMemoryModel(GazerContext& context, LLVMFrontendSettings& settings, const llvm::DataLayout& dl)
    -> std::unique_ptr<MemoryModel>
{
    return std::make_unique<DummyMemoryModel>(context, settings, dl);
}
