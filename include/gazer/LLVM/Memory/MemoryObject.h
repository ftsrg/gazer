#ifndef GAZER_LLVM_MEMORY_MEMORYOBJECT_H
#define GAZER_LLVM_MEMORY_MEMORYOBJECT_H

#include "gazer/Core/Expr.h"
#include "gazer/LLVM/TypeTranslator.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>
#include <llvm/Pass.h>
#include <llvm/IR/ValueHandle.h>

namespace gazer
{

/// Represents the allocation type of a memory object.
enum class MemoryAllocType
{
    Unknown = 0,    ///< allocated in an unknown source
    Global,         ///< allocated as a global variable
    Alloca,         ///< allocated by an alloca instruction
    Heap            ///< allocated on the heap (e.g. 'malloc')
};

enum class MemoryObjectType
{
    Unknown = 0,    ///< will be represented as an unstructured array of bytes
    Scalar,         ///< a primitive scalar type, cannot have subobjects
    Array,          ///< an array of some known type, subobjects can be array elements
    Struct          ///< a struct, subobjects can be struct fields
};

class MemoryObject;
class MemoryObjectDef;
class MemoryObjectUse;
class MemoryObjectPhi;

/// \brief A memory object is a continuous area of memory which does
/// not overlap with other memory objects.
class MemoryObject
{
    friend class MemorySSABuilder;
public:
    using MemoryObjectSize = uint64_t;
    static constexpr MemoryObjectSize UnknownSize = ~uint64_t(0);

public:
    MemoryObject(
        unsigned id,
        MemoryAllocType allocType,
        MemoryObjectType objectType,
        MemoryObjectSize size,
        llvm::Type* valueType,
        llvm::StringRef name = ""
    ) :
        mId(id), mAllocType(allocType), mObjectType(objectType), mSize(size),
        mValueType(valueType), mName(name)
    {}

    void print(llvm::raw_ostream& os) const;

    ~MemoryObject();

private:
    void addDefinition(MemoryObjectDef* def);

private:
    unsigned mId;
    MemoryAllocType mAllocType;
    MemoryObjectType mObjectType;
    MemoryObjectSize mSize;

    llvm::Type* mValueType;
    std::string mName;

    std::vector<std::unique_ptr<MemoryObjectDef>> mDefs;
};

class MemoryObjectDef
{
public:
    enum Kind
    {
        Def_Entry,
        Def_Store,
        Def_Call,
        Def_Phi
    };

protected:
    MemoryObjectDef(MemoryObject* object, unsigned version, Kind kind)
        : mObject(object), mVersion(version), mKind(kind)
    {}

public:
    MemoryObject* getObject() const { return mObject; }
    unsigned getVersion() const { return mVersion; }

private:
    MemoryObject* mObject;
    Kind mKind;
    unsigned mVersion;
};

namespace memory
{

class EntryDef : public MemoryObjectDef
{
public:
    EntryDef(MemoryObject* object, unsigned int version)
        : MemoryObjectDef(object, version, MemoryObjectDef::Def_Entry)
    {}
};

class StoreDef : public MemoryObjectDef
{
public:
    StoreDef(MemoryObject* object, unsigned int version, llvm::StoreInst& store)
        : MemoryObjectDef(object, version, MemoryObjectDef::Def_Store),
        mStore(&store)
    {}

private:
    llvm::PoisoningVH<llvm::StoreInst> mStore;
};

} // end namespace gazer::memory

/// Helper class for building memory SSA form.
class MemorySSABuilder
{
public:
    explicit MemorySSABuilder(const llvm::DataLayout& dl)
        : mDataLayout(dl)
    {}

    MemoryObject* createMemoryObject(
        MemoryAllocType allocType, MemoryObjectType objectType,
        MemoryObject::MemoryObjectSize size, llvm::Type* valueType,
        llvm::StringRef name = ""
    );

    memory::EntryDef* getEntryDefinition(MemoryObject* object);
    memory::StoreDef* addDefinition(MemoryObject* object, llvm::StoreInst& inst);

    [[nodiscard]] const llvm::DataLayout& getDataLayout() const { return mDataLayout; }

public: // TODO
    const llvm::DataLayout& mDataLayout;
    std::vector<std::unique_ptr<MemoryObject>> mObjects;
    unsigned mId = 0;
};

// LLVM Pass
//-----------------------------------------------------------------------------

class MemoryObjectPass : public llvm::FunctionPass
{
public:
    static char ID;

    MemoryObjectPass()
        : FunctionPass(ID)
    {}

    bool runOnFunction(llvm::Function& module) override;

    llvm::StringRef getPassName() const override {
        return "Memory object analysis";
    }
};

} // end namespace gazer

#endif
