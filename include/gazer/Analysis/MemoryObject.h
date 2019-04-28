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

/// Helper struct to represent a memory object - SSA version pair.
class MemoryObjectVersion
{
public:
    MemoryObjectVersion(MemoryObject* object, unsigned version)
        : mObject(object), mVersion(version)
    {}

    MemoryObjectVersion(const MemoryObjectVersion&) = default;
    MemoryObjectVersion& operator=(const MemoryObjectVersion&) = default;

    MemoryObject* getObject() const { return mObject; }
    unsigned getVersion() const { return mVersion; }
private:
    MemoryObject* mObject;
    unsigned mVersion;
};

class MemoryObjectDef
{
public:
    MemoryObjectDef(std::vector<MemoryObjectVersion> defs)
        : mDefinedObjects(defs)
    {}

    using iterator = std::vector<MemoryObjectVersion>::iterator;
    iterator begin() { return mDefinedObjects.begin(); }
    iterator end() { return mDefinedObjects.end(); }

protected:
    std::vector<MemoryObjectVersion> mDefinedObjects;
};

class MemoryObjectPhi : public MemoryObjectDef
{
public:
    MemoryObjectPhi(llvm::BasicBlock* bb, std::vector<MemoryObjectVersion> defs)
        : mBasicBlock(bb), MemoryObjectDef(defs)
    {}
private:
    llvm::BasicBlock* mBasicBlock;
};

class MemoryObjectInst : public MemoryObjectDef
{
public:
    MemoryObjectInst(llvm::Instruction* inst, std::vector<MemoryObjectVersion> defs)
        : mInstruction(inst), MemoryObjectDef(defs)
    {}
private:
    llvm::Instruction* mInstruction;
};

class MemoryObjectUse
{

};



/// \brief A memory object is a continuous area of memory which does
/// not overlap with other memory objects.
///
/// A memory object may have child objects, e.g. a struct may contain a child
/// object for each of its fields or an array for its elements. Child objects
/// are constructed and inserted on-demand by MemoryObjectAnalysis.
class MemoryObject
{
    friend class MemoryObjectAnalysis;

    using MemoryObjectSize = uint64_t;
    static constexpr MemoryObjectSize UnknownMemoryObjectSize = ~uint64_t(0);
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

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const MemoryObject& memObject);

} // end namespace gazer


#endif
