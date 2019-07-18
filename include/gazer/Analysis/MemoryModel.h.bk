#ifndef GAZER_ANALYSIS_MEMORYMODEL_H
#define GAZER_ANALYSIS_MEMORYMODEL_H

#include <llvm/IR/Type.h>
#include <llvm/ADT/Twine.h>
#include <llvm/IR/Instructions.h>

namespace gazer
{

enum class MemoryAllocation
{
    Unknown = 0,
    Global,
    Alloca,
    Heap
};

enum class MemoryObjectType
{
    Unknown = 0,
    Scalar,
    Array,
    Struct
};

class MemoryObject;

class MemoryObjectAccess;
class MemoryObjectDef;
class MemoryObjectUse;
class MemoryObjectPhi;

/// A memory object is a continuous area of memory which does
/// not overlap with other memory objects.
class MemoryObject
{
    using MemoryObjectSize = uint64_t;
    static constexpr MemoryObjectSize UnknownSize = ~uint64_t(0);
public:
    MemoryObject(
        unsigned id,
        MemoryAllocation allocType,
        MemoryObjectType objectType,
        MemoryObjectSize size,
        llvm::Type* valueType,
        llvm::Twine name = ""
    )
        : mID(id), mAllocType(allocType), mSize(size),
        mObjectType(objectType), mValueType(valueType), mName(name.str())
    {}

    MemoryAllocation getAllocationType() const { return mAllocType; }
    MemoryObjectSize getSize() const { return mSize; }

    void print(llvm::raw_ostream& os) const;

private:
    unsigned mID;
    MemoryObjectSize mSize;
    MemoryAllocation mAllocType;
    MemoryObjectType mObjectType;
    llvm::Type* mValueType;
    std::string mName;
};

/// Represents a (potentially) memory-modifying instruction.
class MemoryInstruction
{
public:
    llvm::Instruction* getInstruction() const { return mInst; }
private:
    llvm::Instruction* mInst;
    llvm::SmallVector<MemoryObjectAccess*, 1> mMemoryAccesses;
};

class MemoryObjectAccess
{
public:
    enum Kind
    {
        Def,
        Use,
        Phi
    };

public:
    MemoryObject* getObject() const { return mObject; }
    MemoryObjectDef* getReachingDef() { return mReachingDefinition; }

private:
    Kind mKind;
    MemoryObject* mObject;
    MemoryObjectDef* mReachingDefinition;
};

class MemoryObjectDef : public MemoryObjectAccess
{
public:
    unsigned getVersion() const { return mVersion; }
private:
    unsigned mVersion;
};

class MemoryObjectUse : public MemoryObjectAccess
{
public:

private:

};

class MemoryObjectPhi : public MemoryObjectAccess
{
public:
    size_t getNumIncomingValues() const { return mIncoming.size(); }

    MemoryObjectAccess* getIncomingValue(size_t i) {
        assert(i < mIncoming.size());
        return mIncoming[i].second;
    }

private:
    std::vector<std::pair<llvm::BasicBlock*, MemoryObjectAccess*>> mIncoming;
};

class MemoryModel
{

};

}

#endif
