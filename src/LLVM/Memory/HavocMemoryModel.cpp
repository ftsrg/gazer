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
///
/// \file This file defines the HavocMemoryModel class, a memory model
/// implementation that returns 'Undef' for all memory instructions.
///
//===----------------------------------------------------------------------===//

#include "gazer/LLVM/Memory/MemoryModel.h"
#include "gazer/Core/LiteralExpr.h"

using namespace gazer;

namespace
{

class HavocMemoryModel :
    public MemoryModel, public MemoryInstructionHandler, public MemoryTypeTranslator
{
public:
    HavocMemoryModel(GazerContext& context)
        : MemoryTypeTranslator(context)
    {}

    MemoryInstructionHandler& getMemoryInstructionHandler(llvm::Function& function) override {
        return *this;
    }

    MemoryTypeTranslator& getMemoryTypeTranslator() override {
        return *this;
    }

    gazer::Type& handlePointerType(const llvm::PointerType* type) override {
        return this->ptrType();
    }
    gazer::Type& handleArrayType(const llvm::ArrayType* type) override {
        return this->ptrType();
    }

    ExprPtr handleAlloca(
        const llvm::AllocaInst& alloc,
        llvm2cfa::GenerationStepExtensionPoint& ep) override
    {
        return ptrValue();        
    }

    ExprPtr handlePointerValue(const llvm::Value* value) override { return ptrValue(); }
    ExprPtr handlePointerCast(const llvm::CastInst& cast, const ExprPtr& origPtr) override {
        return ptrValue();
    }
    ExprPtr handleGetElementPtr(
        const llvm::GetElementPtrInst& gep,
        llvm::ArrayRef<ExprPtr> ops) override
    {
        return ptrValue();
    }

    ExprPtr handleConstantDataArray(
        const llvm::ConstantDataArray* cda, llvm::ArrayRef<ExprRef<LiteralExpr>> elems) override
    {
        return ptrValue();
    }

    ExprPtr handleZeroInitializedAggregate(const llvm::ConstantAggregateZero* caz) override
    {
        return ptrValue();
    }

    void handleStore(
        const llvm::StoreInst& store,
        llvm2cfa::GenerationStepExtensionPoint& ep) override {}

    ExprPtr handleLoad(
        const llvm::LoadInst& load, llvm2cfa::GenerationStepExtensionPoint& ep) override
    {
        auto variable = ep.getVariableFor(&load);
        return UndefExpr::Get(variable->getType());
    }

    void handleCall(
        const llvm::CallBase* call,
        llvm2cfa::GenerationStepExtensionPoint& callerEp,
        llvm2cfa::AutomatonInterfaceExtensionPoint& calleeEp,
        llvm::SmallVectorImpl<VariableAssignment>& inputAssignments,
        llvm::SmallVectorImpl<VariableAssignment>& outputAssignments) override {}

    ExprPtr isValidAccess(llvm::Value* ptr, const ExprPtr& expr) override {
        return BoolLiteralExpr::True(BoolType::Get(mContext));
    }

private:
    gazer::Type& ptrType() const { return BoolType::Get(mContext); }
    ExprPtr ptrValue() const { return UndefExpr::Get(this->ptrType()); }

};

} // namespace

auto gazer::CreateHavocMemoryModel(GazerContext& context) -> std::unique_ptr<MemoryModel>
{
    return std::make_unique<HavocMemoryModel>(context);
}
