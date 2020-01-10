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
#include "GazerContextImpl.h"

#include "gazer/Core/Type.h"

#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/Support/FormatVariadic.h>

#include <algorithm>
#include <gazer/Core/GazerContext.h>

using namespace gazer;

static std::string getArrayTypeStr(Type& indexType, Type& elemType)
{
    return llvm::formatv("[{0} -> {1}]", indexType.getName(), elemType.getName());
}

std::string Type::getName() const
{
    switch (getTypeID()) {
        case BoolTypeID:
            return "Bool";
        case BvTypeID: {
            auto intType = llvm::cast<BvType>(this);
            return "Bv" + std::to_string(intType->getWidth());
        }
        case IntTypeID:
            return "Int";
        case RealTypeID:
            return "Real";
        case FloatTypeID: {
            auto fltTy = llvm::cast<FloatType>(this);
            return "Float" + std::to_string(fltTy->getWidth());
        }
        case ArrayTypeID: {
            auto arrayType = llvm::cast<ArrayType>(this);
            return getArrayTypeStr(
                arrayType->getIndexType(), arrayType->getElementType()
            );
        }
        /*
        case FunctionTypeID: {
            auto funcType = llvm::cast<FunctionType>(this);
            return getFunctionTypeStr(
                funcType->getReturnType(), funcType->args()
            );
        } */
    }

    llvm_unreachable("Invalid TypeID");
}

llvm::raw_ostream& gazer::operator<<(llvm::raw_ostream& os, const Type& type)
{
    return os << type.getName();
}

bool Type::equals(const Type* other) const
{
    if (&mContext != &other->mContext) {
        return false;
    }

    if (getTypeID() != other->getTypeID()) {
        return false;
    }
    
    if (getTypeID() == BvTypeID) {
        auto left = llvm::dyn_cast<BvType>(this);
        auto right = llvm::dyn_cast<BvType>(other);

        return left->getWidth() == right->getWidth();
    }
    
    if (getTypeID() == FloatTypeID) {
        auto left = llvm::dyn_cast<FloatType>(this);
        auto right = llvm::dyn_cast<FloatType>(other);

        return left->getPrecision() == right->getPrecision();
    }
    
    if (getTypeID() == ArrayTypeID) {
        auto left = llvm::dyn_cast<ArrayType>(this);
        auto right = llvm::dyn_cast<ArrayType>(other);

        return left->getIndexType() == right->getIndexType()
            && left->getElementType() == right->getElementType();
    }

    return true;
}

auto BoolType::Get(GazerContext& context) -> BoolType&
{
    return context.pImpl->BoolTy;
}

auto IntType::Get(GazerContext& context) -> IntType&
{
    return context.pImpl->IntTy;
}

auto RealType::Get(GazerContext& context) -> RealType&
{
    return context.pImpl->RealTy;
}

auto BvType::Get(GazerContext& context, unsigned width) -> BvType&
{
    auto& pImpl = context.pImpl;

    switch (width) {
        case 8:  return pImpl->Bv8Ty;
        case 16: return pImpl->Bv16Ty;
        case 32: return pImpl->Bv32Ty;
        case 64: return pImpl->Bv64Ty;
        default:
            break;
    }

    auto result = pImpl->BvTypes.find(width);
    if (result == pImpl->BvTypes.end()) {
        auto ptr = new BvType(context, width);
        pImpl->BvTypes.emplace(width, ptr);

        return *ptr;
    }

    return *result->second;
}

auto FloatType::Get(
    GazerContext& context, FloatType::FloatPrecision precision) -> FloatType&
{
    switch (precision) {
        case Half: return context.pImpl->FpHalfTy;
        case Single: return context.pImpl->FpSingleTy;
        case Double: return context.pImpl->FpDoubleTy;
        case Quad: return context.pImpl->FpQuadTy;
    }

    llvm_unreachable("Invalid floating-point type");
}

const llvm::fltSemantics& FloatType::getLLVMSemantics() const
{
    switch (getPrecision()) {
        case Half: return llvm::APFloat::IEEEhalf();
        case Single: return llvm::APFloat::IEEEsingle();
        case Double: return llvm::APFloat::IEEEdouble();
        case Quad: return llvm::APFloat::IEEEquad();
    }

    llvm_unreachable("Invalid floating-point type");
}

unsigned FloatType::getExponentWidth() const
{
    switch (getPrecision()) {
        case Half: return ExponentBitsInHalfTy;
        case Single: return ExponentBitsInSingleTy;
        case Double: return ExponentBitsInDoubleTy;
        case Quad: return ExponentBitsInQuadTy;
    }

    llvm_unreachable("Invalid floating-point precision!");
}

unsigned FloatType::getSignificandWidth() const
{
    switch (getPrecision()) {
        case Half: return SignificandBitsInHalfTy;
        case Single: return SignificandBitsInSingleTy;
        case Double: return SignificandBitsInDoubleTy;
        case Quad: return SignificandBitsInQuadTy;
    }

    llvm_unreachable("Invalid floating-point precision!");
}

CompositeType::CompositeType(GazerContext& context, TypeID id, std::vector<Type*> subTypes)
    : Type(context, id), mSubTypes(std::move(subTypes))
{
    assert(!mSubTypes.empty());
    assert(std::all_of(mSubTypes.begin(), mSubTypes.end(), [&context](auto& ty) {
        return ty->getContext() == context;
    }));
}

ArrayType::ArrayType(GazerContext& context, std::vector<Type*> types)
    : CompositeType(context, ArrayTypeID, types)
{
    assert(types.size() == 2);
}

ArrayType& ArrayType::Get(Type& indexType, Type& elementType)
{
    auto& ctx = indexType.getContext();
    auto& pImpl = ctx.pImpl;

    std::vector<Type*> subtypes = { &indexType, &elementType };

    auto result = pImpl->ArrayTypes.find(subtypes);
    if (result == pImpl->ArrayTypes.end()) {
        auto ptr = new ArrayType(ctx, subtypes);
        pImpl->ArrayTypes.emplace(subtypes, ptr);

        return *ptr;
    }

    return *result->second;
}

TupleType& TupleType::Get(std::vector<Type*> subtypes)
{
    assert(!subtypes.empty());
    assert(subtypes.size() >= 2);
    assert(std::none_of(subtypes.begin(), subtypes.end(), [](Type* t) {
        return t->isTupleType();
    }) && "Nested tuples are not supported!");

    auto& ctx = subtypes[0]->getContext();
    auto& pImpl = ctx.pImpl;

    auto result = pImpl->TupleTypes.find(subtypes);
    if (result == pImpl->TupleTypes.end()) {
        auto ptr = new TupleType(ctx, subtypes);
        pImpl->TupleTypes.emplace(subtypes, ptr);

        return *ptr;
    }

    return *result->second;
}

