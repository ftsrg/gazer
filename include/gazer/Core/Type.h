#ifndef _GAZER_CORE_TYPE_H
#define _GAZER_CORE_TYPE_H

#include <llvm/Support/Casting.h>
#include <llvm/ADT/iterator.h>

#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <iosfwd>

namespace llvm {
    struct fltSemantics;
    class raw_ostream;
}

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
        BvTypeID,
        FloatTypeID,

        // Composite types
        //PointerTypeID,
        ArrayTypeID,
        //StructTypeID,
        //FunctionTypeID
    };

    static constexpr int FirstPrimitive = BoolTypeID;
    static constexpr int LastPrimitive = FloatTypeID;
    //static constexpr int FirstComposite = PointerTypeID;
    static constexpr int FirstComposite = ArrayTypeID;
    static constexpr int LastComposite = ArrayTypeID;
protected:
    explicit Type(TypeID id)
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
    bool isBvType() const { return getTypeID() == BvTypeID; }
    bool isFloatType() const { return getTypeID() == FloatTypeID; }
    bool isArrayType() const { return getTypeID() == ArrayTypeID; }

    //bool isPointerType() const { return getTypeID() == PointerTypeID; }

    bool equals(const Type* other) const;

    bool operator==(const Type& other) const { return equals(&other); }
    bool operator!=(const Type& other) const { return !equals(&other); }

    std::string getName() const;

private:
    TypeID mTypeID;
};

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const Type& type);

//*========= Types =========*//

class BoolType final : public Type
{
protected:
    BoolType()
        : Type(BoolTypeID)
    {}
public:
    static BoolType& get() {
        static BoolType instance;
        return instance;
    }

    static bool classof(const Type* type) {
        return type->getTypeID() == BoolTypeID;
    }
};

class BvType final : public Type
{
protected:
    BvType(unsigned width)
        : Type(BvTypeID), mWidth(width)
    {}
public:
    unsigned getWidth() const { return mWidth; }

    static BvType& get(unsigned width);

    static bool classof(const Type* type) {
        return type->getTypeID() == BvTypeID;
    }

    static bool classof(const Type& type) {
        return type.getTypeID() == BvTypeID;
    }
private:
    unsigned mWidth;
    static BvType Bv1Ty, Bv8Ty, Bv16Ty, Bv32Ty, Bv64Ty;
};

/**
 * Unbounded, mathematical integer type.
 * For compability reasons, we also define the bit width of this type.
 */
class IntType final : public Type
{
protected:
    IntType(unsigned width)
        : Type(IntTypeID), mWidth(width)
    {}
public:
    unsigned getWidth() const { return mWidth; }

    static IntType& get(unsigned width);

    static bool classof(const Type* type) {
        return type->getTypeID() == IntTypeID;
    }

    static bool classof(const Type& type) {
        return type.getTypeID() == IntTypeID;
    }
private:
    unsigned mWidth;
    static IntType Int1Ty, Int8Ty, Int16Ty, Int32Ty, Int64Ty;
};

/**
 * Represents an IEEE-754 floating point type.
 */
class FloatType final : public Type
{
public:
    enum FloatPrecision
    {
        Half = 16,
        Single = 32,
        Double = 64,
        Quad = 128
    };

protected:
    FloatType(FloatPrecision precision)
        : Type(FloatTypeID), mPrecision(precision)
    {}

public:
    FloatPrecision getPrecision() const { return mPrecision; }
    const llvm::fltSemantics& getLLVMSemantics() const;
    unsigned getWidth() const { return mPrecision; }

    static FloatType& get(FloatPrecision precision);

    static bool classof(const Type* type) {
        return type->getTypeID() == FloatTypeID;
    }
    static bool classof(const Type& type) {
        return type.getTypeID() == FloatTypeID;
    }

private:
    FloatPrecision mPrecision;
    static FloatType HalfTy, SingleTy, DoubleTy, QuadTy;
};

/**
 * Represents an array type with arbitrary index and element types.
 */
class ArrayType final : public Type
{
    ArrayType(Type* indexType, Type* elementType)
        : Type(ArrayTypeID), mIndexType(indexType), mElementType(elementType)
    {
        assert(indexType != nullptr);
        assert(elementType != nullptr);
    }

public:
    Type& getIndexType() const { return *mIndexType; }
    Type& getElementType() const { return *mElementType; }

    static ArrayType& get(Type& indexType, Type& elementType);

    static bool classof(const Type* type) {
        return type->getTypeID() == ArrayTypeID;
    }

private:
    Type* mIndexType;
    Type* mElementType;
};

/*
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

    static FunctionType& get(Type* returnType, std::vector<Type*> args);

    static FunctionType& get(Type* returnType) {
        return FunctionType::get(returnType, {});
    }

private:
    Type* mReturnType;
    std::vector<Type*> mArgTypes;
};
*/
}

#endif
