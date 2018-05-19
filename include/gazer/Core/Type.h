#ifndef _GAZER_CORE_TYPE_H
#define _GAZER_CORE_TYPE_H

#include <llvm/Support/Casting.h>
#include <llvm/ADT/iterator.h>

#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <iosfwd>

namespace gazer
{

/**
 * Base class for all gazer types.
 */
class Type
{
public:
    enum TypeID
    {
        // Primitive types
        BoolTypeID = 0,
        IntTypeID,
        //RealTypeID,

        // Composite types
        ArrayTypeID,
        //StructTypeID,
        FunctionTypeID
    };

    static constexpr int FirstPrimitive = BoolTypeID;
    static constexpr int LastPrimitive = IntTypeID;
    static constexpr int FirstComposite = ArrayTypeID;
    static constexpr int LastComposite = FunctionTypeID;
protected:
    Type(TypeID id)
        : mTypeID(id)
    {}

public:
    Type(const Type&) = delete;
    Type& operator=(const Type&) = delete;

    TypeID getTypeID() const { return mTypeID; }
    
    bool isPrimitiveType() const {
        return mTypeID >= FirstPrimitive && mTypeID <= LastPrimitive;
    }
    bool isCompositeType() const {
        return mTypeID >= FirstComposite && mTypeID <= LastComposite;
    }

    bool isBoolType() const { return getTypeID() == BoolTypeID; }
    bool isIntType() const { return getTypeID() == IntTypeID; }
    //bool isRealType() const { return getTypeID() == RealTypeID; }

    bool equals(const Type* other) const;

    bool operator==(const Type& other) const { return equals(&other); }
    bool operator!=(const Type& other) const { return !equals(&other); }

    std::string getName() const;

private:
    TypeID mTypeID;
};

/**
 * Exception class for type cast errors.
 */
class TypeCastError : public std::logic_error {
public:
    TypeCastError(const Type* from, const Type* to, std::string message = "");
    TypeCastError(const Type& from, const Type& to, std::string message = "");
    TypeCastError(std::string message);
};

inline void check_type(const Type& from, const Type& to) {
    if (from != to) {
        throw TypeCastError(from, to);
    }
}

inline void check_type(const Type* from, const Type* to) {
    throw TypeCastError(from, to);
}

//*========= Types =========*//

class BoolType final : public Type
{
protected:
    BoolType()
        : Type(BoolTypeID)
    {}
public:
    static BoolType* get() {
        static BoolType instance;
        return &instance;
    }

    static bool classof(const Type* type) {
        return type->getTypeID() == BoolTypeID;
    }
};

class IntType final : public Type
{
protected:
    IntType()
        : Type(IntTypeID)
    {}
public:
    static IntType* get() {
        static IntType instance;
        return &instance;
    }

    static bool classof(const Type* type) {
        return type->getTypeID() == IntTypeID;
    }
};

class ArrayType final : public Type
{
    ArrayType(Type* indexType, Type* elementType)
        : Type(ArrayTypeID), mIndexType(indexType), mElementType(elementType)
    {}

public:
    Type* getIndexType() const { return mIndexType; }
    Type* getElementType() const { return mElementType; }

    static ArrayType* get(Type* indexType, Type* elementType);

    static bool classof(const Type* type) {
        return type->getTypeID() == ArrayTypeID;
    }

private:
    Type* mIndexType;
    Type* mElementType;
};

class FunctionType final : public Type
{
    FunctionType(Type* returnType, std::vector<Type*> args = {})
        : Type(FunctionTypeID), mReturnType(returnType), mArgTypes(args)
    {}

    template<class Iter>
    FunctionType(Type* returnType, Iter argBegin, Iter argEnd)
        : Type(FunctionTypeID), mReturnType(returnType), mArgTypes(argBegin, argEnd)
    {}

public:
    Type* getReturnType() const { return mReturnType; }

    size_t getNumArgs() const { return mArgTypes.size(); }

    using arg_iterator = std::vector<Type*>::const_iterator;
    arg_iterator arg_begin() const { return mArgTypes.begin(); }
    arg_iterator arg_end() const { return mArgTypes.end(); }

    llvm::iterator_range<arg_iterator> args() const {
        return llvm::make_range(arg_begin(), arg_end());
    }

    static bool classof(const Type* type) {
        return type->getTypeID() == FunctionTypeID;
    }

    static FunctionType* get(Type* returnType, std::vector<Type*> args);

    static FunctionType* get(Type* returnType) {
        return FunctionType::get(returnType, {});
    }

private:
    Type* mReturnType;
    std::vector<Type*> mArgTypes;
};

}

#endif
