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

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>

using namespace gazer;

namespace
{

class BasicMemoryModel : public MemoryModel
{
public:
    BasicMemoryModel(GazerContext& context, const LLVMFrontendSettings& settings)
        : MemoryModel(context, settings)
    {}

    void findMemoryObjects(llvm::Function& function, MemorySSABuilder& builder) override;

    ExprPtr handleLoad(const llvm::LoadInst& load) override
    {
        return gazer::ExprPtr();
    }

    ExprPtr handleGetElementPtr(const llvm::GEPOperator& gep) override
    {
        return gazer::ExprPtr();
    }

    ExprPtr handleAlloca(const llvm::AllocaInst& alloc) override
    {
        return gazer::ExprPtr();
    }

    ExprPtr handlePointerCast(const llvm::CastInst& cast) override
    {
        return gazer::ExprPtr();
    }

    ExprPtr handlePointerValue(const llvm::Value* value) override
    {
        return gazer::ExprPtr();
    }

    std::optional<VariableAssignment> handleStore(const llvm::StoreInst& store, ExprPtr pointer, ExprPtr value) override
    {
        return std::optional<VariableAssignment>();
    }

    Type& handlePointerType(const llvm::PointerType* type) override
    {
        return BvType::Get(mContext, 32);
    }

    Type& handleArrayType(const llvm::ArrayType* type) override
    {
        return BvType::Get(mContext, 32);
    }
};

void BasicMemoryModel::findMemoryObjects(llvm::Function& function, MemorySSABuilder& builder)
{
    // Each function will have a memory object made from this global variable.
    for (llvm::GlobalVariable& gv : function.getParent()->globals()) {
        auto gvTy = gv.getType()->getPointerElementType();
        builder.createMemoryObject(
            MemoryAllocType::Global,
            MemoryObjectType::Scalar,
            builder.getDataLayout().getTypeAllocSize(gvTy),
            gvTy,
            gv.getName()
        );
    }
}

} // end anonymous namespace

auto gazer::CreateBasicMemoryModel(GazerContext& context, const LLVMFrontendSettings& settings) -> std::unique_ptr<MemoryModel>
{
    return std::make_unique<BasicMemoryModel>(context, settings);
}
