#include "gazer/Core/Type.h"

#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/Support/FormatVariadic.h>

#include <algorithm>

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
        case FloatTypeID: {
            auto fltTy = llvm::cast<FloatType>(this);
            return "Float" + std::to_string(fltTy->getWidth());
        }
        case ArrayTypeID: {
            auto arrayType = llvm::cast<ArrayType>(this);
            return getArrayTypeStr(
                arrayType->getIndexType(), arrayType->getElementType()
            );
        }/*
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

BvType BvType::Bv1Ty(1);
BvType BvType::Bv8Ty(8);
BvType BvType::Bv16Ty(16);
BvType BvType::Bv32Ty(32);
BvType BvType::Bv64Ty(64);

IntType IntType::Int1Ty(1);
IntType IntType::Int8Ty(8);
IntType IntType::Int16Ty(16);
IntType IntType::Int32Ty(32);
IntType IntType::Int64Ty(64);

IntType& IntType::get(unsigned width)
{
    static llvm::DenseMap<unsigned, std::unique_ptr<IntType>> Instances;

    switch (width) {
        case 1: return Int1Ty;
        case 8: return Int8Ty;
        case 16: return Int16Ty;
        case 32: return Int32Ty;
        case 64: return Int64Ty;
    }

    auto result = Instances.find(width);
    if (result == Instances.end()) {
        auto pair = Instances.try_emplace(width, new IntType(width));
        return *pair.first->second;
    }

    return *result->second;
}

BvType& BvType::get(unsigned width)
{
    static llvm::DenseMap<unsigned, std::unique_ptr<BvType>> Instances;

    switch (width) {
        case 1: return Bv1Ty;
        case 8: return Bv8Ty;
        case 16: return Bv16Ty;
        case 32: return Bv32Ty;
        case 64: return Bv64Ty;
    }

    auto result = Instances.find(width);
    if (result == Instances.end()) {
        auto pair = Instances.try_emplace(width, new BvType(width));
        return *pair.first->second;
    }

    return *result->second;
}

FloatType FloatType::HalfTy(Half);
FloatType FloatType::SingleTy(Single);
FloatType FloatType::DoubleTy(Double);
FloatType FloatType::QuadTy(Quad);

FloatType& FloatType::get(FloatType::FloatPrecision precision)
{
    switch (precision) {
        case Half: return HalfTy;
        case Single: return SingleTy;
        case Double: return DoubleTy;
        case Quad: return QuadTy;
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

ArrayType& ArrayType::get(Type& indexType, Type& elementType)
{
    // TODO: This is surely not the best way to do this
    static llvm::StringMap<std::unique_ptr<ArrayType>> Instances;
    
    auto key = getArrayTypeStr(indexType, elementType);
    auto result = Instances.find(key);

    if (result == Instances.end()) {
        auto pair = Instances.try_emplace(key, new ArrayType(&indexType, &elementType));
        return *pair.first->second;
    }

    return *result->second;
}
/*
FunctionType& FunctionType::get(Type* returnType, std::vector<Type*> args)
{
    static llvm::StringMap<std::unique_ptr<FunctionType>> Instances;

    // TODO: This is surely not the best way to do this
    auto key = getFunctionTypeStr(returnType, llvm::make_range(args.begin(), args.end()));

    auto result = Instances.find(key);
    if (result == Instances.end()) {
        auto pair = Instances.try_emplace(key, new FunctionType(returnType, args));
        return *pair.first->second.get();
    }

    return *result->second.get();
}
*/