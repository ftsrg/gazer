#ifndef GAZER_CORE_TYPE_H
#define GAZER_CORE_TYPE_H

#include "gazer/Core/GazerContext.h"

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

class GazerContext;

/// Base class for all gazer types.
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
        RealTypeID,

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
    explicit Type(GazerContext& context, TypeID id)
        : mContext(context), mTypeID(id)
    {}

public:
    Type(const Type&) = delete;
    Type& operator=(const Type&) = delete;

    [[nodiscard]] GazerContext& getContext() const { return mContext; }
    [[nodiscard]] TypeID getTypeID() const { return mTypeID; }
    
    [[nodiscard]] bool isPrimitiveType() const {
        return mTypeID >= FirstPrimitive && mTypeID <= LastPrimitive;
    }
    [[nodiscard]] bool isCompositeType() const {
        return mTypeID >= FirstComposite && mTypeID <= LastComposite;
    }

    [[nodiscard]] bool isBoolType() const { return getTypeID() == BoolTypeID; }
    [[nodiscard]] bool isIntType() const { return getTypeID() == IntTypeID; }
    [[nodiscard]] bool isBvType() const { return getTypeID() == BvTypeID; }
    [[nodiscard]] bool isFloatType() const { return getTypeID() == FloatTypeID; }
    [[nodiscard]] bool isRealType() const { return getTypeID() == RealTypeID; }
    [[nodiscard]] bool isArrayType() const { return getTypeID() == ArrayTypeID; }

    [[nodiscard]] bool isArithmetic() const { return isIntType() || isRealType(); }

    //bool isPointerType() const { return getTypeID() == PointerTypeID; }

    bool equals(const Type* other) const;

    bool operator==(const Type& other) const { return equals(&other); }
    bool operator!=(const Type& other) const { return !equals(&other); }

    [[nodiscard]] std::string getName() const;

private:
    GazerContext& mContext;
    TypeID mTypeID;
};

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const Type& type);

//------------------------ Type declarations --------------------------------//

class BoolType final : public Type
{
    friend class GazerContextImpl;
protected:
    explicit BoolType(GazerContext& context)
        : Type(context, BoolTypeID)
    {}
public:
    static BoolType& Get(GazerContext& context);

    static bool classof(const Type* type) {
        return type->getTypeID() == BoolTypeID;
    }
};

class BvType final : public Type
{
    friend class GazerContextImpl;
protected:
    BvType(GazerContext& context, unsigned width)
        : Type(context, BvTypeID), mWidth(width)
    {}
public:
    [[nodiscard]] unsigned getWidth() const { return mWidth; }

    static BvType& Get(GazerContext& context, unsigned width);

    static bool classof(const Type* type) {
        return type->getTypeID() == BvTypeID;
    }

    static bool classof(const Type& type) {
        return type.getTypeID() == BvTypeID;
    }
private:
    unsigned mWidth;
};

/// Unbounded, mathematical integer type.
class IntType final : public Type
{
    friend class GazerContextImpl;
protected:
    explicit IntType(GazerContext& context)
        : Type(context, IntTypeID)
    {}
public:
    static IntType& Get(GazerContext& context);

    static bool classof(const Type* type) {
        return type->getTypeID() == IntTypeID;
    }

    static bool classof(const Type& type) {
        return type.getTypeID() == IntTypeID;
    }
private:
};

class RealType final : public Type
{
    friend class GazerContextImpl;
protected:
    explicit RealType(GazerContext& context)
        : Type(context, RealTypeID)
    {}
public:
    static RealType& Get(GazerContext& context);

    static bool classof(const Type* type) {
        return type->getTypeID() == RealTypeID;
    }

    static bool classof(const Type& type) {
        return type.getTypeID() == RealTypeID;
    }
};

/// Represents an IEEE-754 floating point type.
class FloatType final : public Type
{
    friend class GazerContextImpl;
public:
    enum FloatPrecision
    {
        Half = 16,
        Single = 32,
        Double = 64,
        Quad = 128
    };

    static constexpr unsigned SignificandBitsInHalfTy   = 11;
    static constexpr unsigned SignificandBitsInSingleTy = 24;
    static constexpr unsigned SignificandBitsInDoubleTy = 53;
    static constexpr unsigned SignificandBitsInQuadTy   = 113;

    static constexpr unsigned ExpBitsInHalfTy   = 5;
    static constexpr unsigned ExpBitsInSingleTy = 8;
    static constexpr unsigned ExpBitsInDoubleTy = 11;
    static constexpr unsigned ExpBitsInQuadTy   = 15;

protected:
    FloatType(GazerContext& context, FloatPrecision precision)
        : Type(context, FloatTypeID), mPrecision(precision)
    {}

public:
    [[nodiscard]] FloatPrecision getPrecision() const { return mPrecision; }
    [[nodiscard]] const llvm::fltSemantics& getLLVMSemantics() const;
    [[nodiscard]] unsigned getWidth() const { return mPrecision; }

    static FloatType& Get(GazerContext& context, FloatPrecision precision);

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

/// Represents an array type with arbitrary index and element types.
class ArrayType final : public Type
{
    ArrayType(GazerContext& context, Type* indexType, Type* elementType)
        : Type(context, ArrayTypeID), mIndexType(indexType), mElementType(elementType)
    {
        assert(indexType != nullptr);
        assert(elementType != nullptr);
    }

public:
    [[nodiscard]] Type& getIndexType() const { return *mIndexType; }
    [[nodiscard]] Type& getElementType() const { return *mElementType; }

    static ArrayType& Get(GazerContext& context, Type& indexType, Type& elementType);

    static bool classof(const Type* type) {
        return type->getTypeID() == ArrayTypeID;
    }

private:
    Type* mIndexType;
    Type* mElementType;
};

}

#endif
