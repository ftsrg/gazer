#include "gazer/Core/Type.h"
#include "gazer/Support/Error.h"

#include <fmt/format.h>
#include <llvm/ADT/StringMap.h>

#include <algorithm>

using namespace gazer;

IntType IntType::Int1Ty(1);
IntType IntType::Int8Ty(8);
IntType IntType::Int16Ty(16);
IntType IntType::Int32Ty(32);
IntType IntType::Int64Ty(64);

static std::string getArrayTypeStr(Type* indexType, Type* elemType)
{
    return fmt::format("[{0} -> {1}]",
        indexType->getName(), elemType->getName());
}

static std::string getFunctionTypeStr(Type* returnType, llvm::iterator_range<FunctionType::arg_iterator> args) {
    auto argPrint = [](Type* type) { return type->getName(); };
    auto range = llvm::make_range(
        llvm::map_iterator(args.begin(), argPrint),
        llvm::map_iterator(args.end(), argPrint)
    );

    return fmt::format("({0}) -> {1}",
        fmt::join(range, ", "), returnType->getName()
    );
}

TypeCastError::TypeCastError(const Type& from, const Type& to, std::string message)
    : TypeCastError(&from, &to, message)
{}

TypeCastError::TypeCastError(const Type* from, const Type* to, std::string message)
    : logic_error(fmt::format(
        "TypeCastError encountered: Invalid cast from type '{0}' to {1}."
        "Error message: {2}",
        from->getName(), to->getName(), message
    ))
{}

TypeCastError::TypeCastError(std::string message)
    : logic_error(message)
{}

std::string Type::getName() const
{
    switch (getTypeID()) {
        case BoolTypeID:
            return "Bool";
        case IntTypeID: {
            auto intType = llvm::dyn_cast<IntType>(this);
            return "Int" + std::to_string(intType->getWidth());
        }
        case ArrayTypeID: {
            auto arrayType = llvm::dyn_cast<ArrayType>(this);
            return getArrayTypeStr(
                arrayType->getIndexType(), arrayType->getElementType()
            );
        }
        case FunctionTypeID: {
            auto funcType = llvm::dyn_cast<FunctionType>(this);
            return getFunctionTypeStr(
                funcType->getReturnType(), funcType->args()
            );
        }
    }

    std::cerr << getTypeID() << "\n";

    llvm_unreachable("Invalid TypeID");
}

bool Type::equals(const Type* other) const
{
    if (getTypeID() != other->getTypeID()) {
        return false;
    } else if (getTypeID() == IntTypeID) {
        auto left = llvm::dyn_cast<IntType>(this);
        auto right = llvm::dyn_cast<IntType>(other);

        return left->getWidth() == right->getWidth();
    } else if (getTypeID() == ArrayTypeID) {
        auto left = llvm::dyn_cast<ArrayType>(this);
        auto right = llvm::dyn_cast<ArrayType>(other);

        return left->getIndexType() == right->getIndexType()
            && left->getElementType() == right->getElementType();
    } else if (getTypeID() == FunctionTypeID) {
        auto left = llvm::dyn_cast<FunctionType>(this);
        auto right = llvm::dyn_cast<FunctionType>(other);

        if (left->getReturnType() != right->getReturnType()) {
            return false;
        }

        return std::equal(
            left->arg_begin(), left->arg_end(),
            right->arg_begin(), right->arg_end(), [](auto lt, auto rt) {
                return lt->equals(rt);
            }
        );
    }

    return true;
}

ArrayType* ArrayType::get(Type* indexType, Type* elementType)
{
    // TODO: This is surely not the best way to do this
    static llvm::StringMap<std::unique_ptr<ArrayType>> Instances;
    
    auto key = getArrayTypeStr(indexType, elementType);
    auto result = Instances.find(key);

    if (result == Instances.end()) {
        auto pair = Instances.try_emplace(key, new ArrayType(indexType, elementType));
        return pair.first->second.get();
    }

    return result->second.get();
}

FunctionType* FunctionType::get(Type* returnType, std::vector<Type*> args)
{
    static llvm::StringMap<std::unique_ptr<FunctionType>> Instances;

    // TODO: This is surely not the best way to do this
    auto key = getFunctionTypeStr(returnType, llvm::make_range(args.begin(), args.end()));

    auto result = Instances.find(key);
    if (result == Instances.end()) {
        auto pair = Instances.try_emplace(key, new FunctionType(returnType, args));
        return pair.first->second.get();
    }

    return result->second.get();
}
