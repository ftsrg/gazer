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

    ExprPtr handleGetElementPtr(const llvm::GEPOperator& gep) override;
    ExprPtr handleAlloca(const llvm::AllocaInst& alloc) override;
    ExprPtr handlePointerCast(const llvm::CastInst& cast) override;
    ExprPtr handlePointerValue(const llvm::Value* value) override;

    void handleStore(
        const llvm::StoreInst& store,
        ExprPtr pointer,
        ExprPtr value,
        llvm2cfa::GenerationStepExtensionPoint& ep
    ) override {}

    void handleBlock(const llvm::BasicBlock& bb, llvm2cfa::GenerationStepExtensionPoint& ep) override {}

    ExprPtr handleLoad(const llvm::LoadInst& load, llvm2cfa::GenerationStepExtensionPoint& ep) override {
        return UndefExpr::Get(translateType(load.getType()));
    }

    gazer::Type& handlePointerType(const llvm::PointerType* type) override;
    gazer::Type& handleArrayType(const llvm::ArrayType* type) override;

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

ExprPtr DummyMemoryModel::handleGetElementPtr(const llvm::GEPOperator& gep)
{
    return UndefExpr::Get(BvType::Get(mContext, 32));
}

ExprPtr DummyMemoryModel::handleAlloca(const llvm::AllocaInst& alloc)
{
    return UndefExpr::Get(BvType::Get(mContext, 32));
}

ExprPtr DummyMemoryModel::handlePointerCast(const llvm::CastInst& cast)
{
    return UndefExpr::Get(BvType::Get(mContext, 32));
}

gazer::Type& DummyMemoryModel::handlePointerType(const llvm::PointerType* type)
{
    return BvType::Get(mContext, 32);
}

gazer::Type& DummyMemoryModel::handleArrayType(const llvm::ArrayType* type)
{
    return ArrayType::Get(mContext, BvType::Get(mContext, 32), BvType::Get(mContext, 8));
}

ExprPtr DummyMemoryModel::handlePointerValue(const llvm::Value* value)
{
    return UndefExpr::Get(BvType::Get(mContext, 32));
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
