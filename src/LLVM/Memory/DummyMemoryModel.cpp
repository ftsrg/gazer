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

void DummyMemoryModel::findMemoryObjects(
    llvm::Function& function,
    MemorySSABuilder& objects
) {
}

ExprPtr DummyMemoryModel::handleLoad(const llvm::LoadInst& load)
{
    return UndefExpr::Get(mTypes.get(load.getType()));
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

std::optional<VariableAssignment> DummyMemoryModel::handleStore(
    const llvm::StoreInst& store, ExprPtr pointer, ExprPtr value
) {
    return std::nullopt;
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
