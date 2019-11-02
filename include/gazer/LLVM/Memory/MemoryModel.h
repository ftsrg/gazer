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
#ifndef GAZER_MEMORY_MEMORYMODEL_H
#define GAZER_MEMORY_MEMORYMODEL_H

#include "gazer/LLVM/Memory/MemoryObject.h"

namespace gazer
{

class MemoryModel
{
public:
    MemoryModel(GazerContext& context, LLVMFrontendSettings settings)
        : mContext(context), mTypes(*this, settings.ints)
    {}

    MemoryModel(const MemoryModel&) = delete;
    MemoryModel& operator=(const MemoryModel&) = delete;

    /// Returns all memory objects found within the given function.
    virtual void findMemoryObjects(llvm::Function& function, MemorySSABuilder& builder) = 0;

    /// Translates the given LoadInst into an assignable expression.
    virtual ExprPtr handleLoad(const llvm::LoadInst& load) = 0;
    virtual ExprPtr handleGetElementPtr(const llvm::GEPOperator& gep) = 0;
    virtual ExprPtr handleAlloca(const llvm::AllocaInst& alloc) = 0;
    virtual ExprPtr handlePointerCast(const llvm::CastInst& cast) = 0;
    virtual ExprPtr handlePointerValue(const llvm::Value* value) = 0;

    virtual std::optional<VariableAssignment> handleStore(
        const llvm::StoreInst& store,
        ExprPtr pointer,
        ExprPtr value
    ) = 0;

    virtual gazer::Type& handlePointerType(const llvm::PointerType* type) = 0;
    virtual gazer::Type& handleArrayType(const llvm::ArrayType* type) = 0;

    GazerContext& getContext() { return mContext; }
    gazer::Type& translateType(const llvm::Type* type) { return mTypes.get(type); }

    virtual ~MemoryModel() {}

protected:
    GazerContext& mContext;
    LLVMTypeTranslator mTypes;
};

/// A dummy memory model which represents the whole memory as one
/// undefined memory object. Load operations return an unknown value and
/// store instructions have no effect. No MemoryObjectPhis are inserted.
class DummyMemoryModel : public MemoryModel
{
public:
    using MemoryModel::MemoryModel;

    void findMemoryObjects(
        llvm::Function& function,
        MemorySSABuilder& builder
    ) override;

    ExprPtr handleLoad(const llvm::LoadInst& load) override;
    ExprPtr handleGetElementPtr(const llvm::GEPOperator& gep) override;
    ExprPtr handleAlloca(const llvm::AllocaInst& alloc) override;
    ExprPtr handlePointerCast(const llvm::CastInst& cast) override;
    ExprPtr handlePointerValue(const llvm::Value* value) override;

    std::optional<VariableAssignment> handleStore(
        const llvm::StoreInst& store, ExprPtr pointer, ExprPtr value
    ) override;

    gazer::Type& handlePointerType(const llvm::PointerType* type) override;
    gazer::Type& handleArrayType(const llvm::ArrayType* type) override;
};

//==-----------------------------------------------------------------------==//
// BasicMemoryModel - a simple memory model which handles local arrays,
// structs, and globals which do not have their address taken. This memory
// model returns undef for all heap operations.
std::unique_ptr<MemoryModel> CreateBasicMemoryModel(GazerContext& context, const LLVMFrontendSettings& settings);

}
#endif //GAZER_MEMORY_MEMORYMODEL_H
