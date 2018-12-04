/// \file
/// \brief This file implements the MemoryObject analysis, which partitions
/// the memory to distinct regions and constructs an SSA form for each of these regions.
/// 
/// A memory object represents a memory region which is guaranteed not to
/// overlap with memory regions of other memory objects.
/// The MemoryObjectAnalysis class discovers these objects and attempts to construct
/// an SSA form for each object.
#ifndef _GAZER_ANALYSIS_MEMORYOBJECT_H
#define _GAZER_ANALYSIS_MEMORYOBJECT_H

#include <llvm/Pass.h>
#include <llvm/IR/Instruction.h>

namespace llvm {
    class MemorySSA;
    class AAResults;
}

namespace gazer
{

/// Represents the allocation type of a memory object.
enum class MemoryAllocationType : unsigned
{
    Unknown = 0,    ///< allocated in an unknown source
    Global,         ///< allocated as a global variable
    Alloca,         ///< allocated by an alloca instruction
    FunctionCall    ///< allocated by a function call (e.g. 'malloc')
};

enum class MemoryObjectType : unsigned
{
    Unknown = 0,    ///< will be represented as an unstructured array of bytes
    Scalar,         ///< a primitive scalar type, cannot have subobjects
    Array,          ///< an array of some known type, subobjects can be array elements
    Struct          ///< a struct, subobjects can be struct fields
};

/// \brief A memory object is a continuous area of memory which does
/// not overlap with other memory objects.
/// 
/// A memory object may have child objects, e.g. a struct may contain a child
/// object for each of its fields or an array for its elements. Child objects
/// are constructed and inserted on-demand by MemoryObjectAnalysis.
class MemoryObject
{
public:
    using MemoryObjectSize = uint64_t;
    static constexpr MemoryObjectSize UnknownSize = ~uint64_t(0);

public:
    MemoryObject(
        unsigned id,
        MemoryAllocationType allocType,
        MemoryObjectSize size,
        MemoryObjectType objectType,
        llvm::Type* valueType,
        llvm::Twine name = ""
    )
        : mID(id), mAllocType(allocType), mSize(size),
        mObjectType(objectType), mValueType(valueType),
        mName(name.str())
    {}

    MemoryAllocationType getAllocationType() const { return mAllocType; }
    MemoryObjectSize getSize() const { return mSize; }

    void print(llvm::raw_ostream& os) const;

public:
    unsigned mID;
private:
    MemoryObjectSize mSize;
    MemoryAllocationType mAllocType;
    MemoryObjectType mObjectType;
    llvm::Type* mValueType;
    std::string mName;
};

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const MemoryObject& memObject);

/// Represents a read or write acces to a memory object.
class MemoryObjectAccess
{
public:
    enum Kind
    {
        Access_Read,
        Access_Write
    };
protected:
    MemoryObjectAccess(Kind kind, MemoryObject* target)
        : mKind(kind), mTarget(target)
    {}

    MemoryObject* getTarget() const { return mTarget; }
    Kind getKind() const { return mKind; }

private:
    MemoryObject* mTarget;
    Kind mKind;
};

class MemoryObjectRead : public MemoryObjectAccess
{
public:

};

class MemoryObjectWrite : public MemoryObjectAccess
{
public:
    /// Returns a list of memory objects potentially modified here
    std::vector<MemoryObject*>& getModifiedObjects() const;

private:
    std::vector<MemoryObject*> mModObjects;
};

/// 
class MemoryObjectAnalysis
{
public:
    /// Constructs a new MemoryObjectAnalysis instance.
    static std::unique_ptr<MemoryObjectAnalysis> Create(
        llvm::Function& function,
        const llvm::DataLayout& dl,
        llvm::MemorySSA& memSSA,
        llvm::AAResults& aa
    );

    /// Finds all memory objects the given pointer may point to.
    void getMemoryObjectsForPointer(llvm::Value* ptr, std::vector<MemoryObject*>& objects);

private:
};

///
class MemoryObjectPass : public llvm::ModulePass
{
public:
    static char ID;

    MemoryObjectPass()
        : ModulePass(ID)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override;
    bool runOnModule(llvm::Module& module) override;
};

}

#endif
