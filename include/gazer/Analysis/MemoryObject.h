/// \file
/// \brief This file implements the MemoryObject analysis, which partitions
/// the memory into distinct regions and constructs an SSA form for each of these regions.
#ifndef GAZER_ANALYSIS_MEMORYOBJECT_H
#define GAZER_ANALYSIS_MEMORYOBJECT_H

#include <llvm/Pass.h>
#include <llvm/IR/Instruction.h>

#include <list>

namespace llvm {
    class MemorySSA;
    class AAResults;
    class DominatorTree;
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

class MemoryObject;

class MemoryObjectDef;

class MemoryUse;

class MemoryObjectUseOrDef
{
protected:
    MemoryObjectUseOrDef(MemoryObject *object)
            : mMemoryObject(object)
    {}

protected:
    MemoryObject *mMemoryObject;
    MemoryObjectDef *mReachingDef;
};

/// Represents a unique definition of a memory object.
class MemoryObjectDef : public MemoryObjectUseOrDef
{
    friend class MemoryObjectAnalysis;
protected:
    MemoryObjectDef(MemoryObject *object, unsigned idx)
        : MemoryObjectUseOrDef(object), mIndex(idx)
    {}

public:
    using use_iterator = std::vector<MemoryUse *>::const_iterator;

    use_iterator use_begin() const
    { return mUses.begin(); }

    use_iterator use_end() const
    { return mUses.end(); }

private:
    MemoryObject *mMemoryObject;
    unsigned mIndex;
    std::vector<MemoryUse *> mUses;
};

/// An LLVM instruction which may modify memory.
class MemoryObjectDefInstruction : public MemoryObjectDef
{
public:
    MemoryObjectDefInstruction(MemoryObject* memoryObject, unsigned idx, llvm::Instruction* inst)
        : MemoryObjectDef(memoryObject, idx), mInst(inst)
    {}

private:
    llvm::Instruction* mInst;
};

class MemoryObjectPhi : public MemoryObjectDef
{
public:
    MemoryObjectPhi(MemoryObject* memoryObject, unsigned idx, llvm::BasicBlock* bb);
};

/// Represents
class MemoryObjectUse : public MemoryObjectUseOrDef
{
    friend class MemoryObjectAnalysis;
public:

public:
    MemoryObjectUse(MemoryObject* object)
        : MemoryObjectUseOrDef(object)
    {}
};

using MemoryObjectSize = uint64_t;
constexpr MemoryObjectSize UnknownMemoryObjectSize = ~uint64_t(0);

/// \brief A memory object is a continuous area of memory which does
/// not overlap with other memory objects.
///
/// A memory object may have child objects, e.g. a struct may contain a child
/// object for each of its fields or an array for its elements. Child objects
/// are constructed and inserted on-demand by MemoryObjectAnalysis.
class MemoryObject
{
    friend class MemoryObjectAnalysis;
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

private:
    MemoryObjectDef* insertDef(llvm::Instruction *inst);
    MemoryObjectUse* insertUse(llvm::Instruction* inst);

public:
    unsigned mID;
private:
    MemoryObjectSize mSize;
    MemoryAllocationType mAllocType;
    MemoryObjectType mObjectType;
    llvm::Type* mValueType;
    std::string mName;

    std::list<MemoryObjectDef*> mDefs;
    std::list<MemoryObjectUse*> mUses;
};

/// \brief Identifies a specific offset within a memory object.
class MemoryCell
{
public:
    MemoryCell(MemoryObject* memoryObject, MemoryObjectSize begin, MemoryObjectSize end = UnknownMemoryObjectSize)
        : mMemoryObject(memoryObject), mBeginOffset(begin), mEndOffset(end)
    {}

private:
    MemoryObject* mMemoryObject;
    MemoryObjectSize mBeginOffset;
    MemoryObjectSize mEndOffset;
};

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const MemoryObject& memObject);

class MemoryPointerVisitor;

/// 
class MemoryObjectAnalysis
{
public:

    /// Constructs a new MemoryObjectAnalysis instance.
    static std::unique_ptr<MemoryObjectAnalysis> Create(
        llvm::Function& function,
        const llvm::DataLayout& dl,
        llvm::DominatorTree& domTree,
        llvm::MemorySSA& memSSA,
        llvm::AAResults& aa
    );

    /// Finds all memory objects the given pointer may point to.
    void getMemoryObjectsForPointer(llvm::Value* ptr, std::vector<MemoryObject*>& objects);

private:

private:
    llvm::DenseMap<llvm::Value*, MemoryObject*> mAllocationSites;
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
