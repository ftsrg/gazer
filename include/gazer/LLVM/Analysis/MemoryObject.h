#ifndef GAZER_LLVM_ANALYSIS_MEMORYOBJECT_H
#define GAZER_LLVM_ANALYSIS_MEMORYOBJECT_H

#include "gazer/Core/Expr.h"
#include "gazer/LLVM/TypeTranslator.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Operator.h>
#include <llvm/Pass.h>

namespace gazer
{


/// Represents the allocation type of a memory object.
enum class MemoryAllocType : unsigned
{
    Unknown = 0,    ///< allocated in an unknown source
    Global,         ///< allocated as a global variable
    Alloca,         ///< allocated by an alloca instruction
    Heap            ///< allocated on the heap (e.g. 'malloc')
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
class MemoryObjectUse;
class MemoryObjectPhi;

/// \brief A memory object is a continuous area of memory which does
/// not overlap with other memory objects.
class MemoryObject
{
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
        std::string name = ""
    ) :
        mId(id), mAllocType(allocType), mObjectType(objectType), mSize(size),
        mValueType(valueType), mName(name)
    {}

    void print(llvm::raw_ostream& os) const;

private:
    unsigned mId;
    MemoryAllocType mAllocType;
    MemoryObjectType mObjectType;
    MemoryObjectSize mSize;

    llvm::Type* mValueType;
    std::string mName;

    std::vector<std::unique_ptr<MemoryObjectDef>> mDefs;
    std::vector<std::unique_ptr<MemoryObjectUse>> mUses;
};

class MemoryObjectDef
{
public:
    MemoryObject* getObject() const { return mObject; }
    unsigned getVersion() const { return mVersion; }

    using use_iterator = std::vector<MemoryObjectUse*>::iterator;
    use_iterator use_begin() { return mUses.begin(); }
    use_iterator use_end() { return mUses.end(); }

private:
    MemoryObject* mObject;
    unsigned mVersion;

    std::vector<MemoryObjectUse*> mUses;
};

class MemoryObjectUse
{
public:

private:
};

class MemoryObjectPhi : public MemoryObjectDef
{
    class MemoryObjectPhiEntry : public MemoryObjectUse
    {
    public:
        
    private:
    };
public:

private:
    std::vector<MemoryObjectPhiEntry*> mEntries;
};

class MemoryModel
{
public:
    explicit MemoryModel(GazerContext& context)
        : mContext(context), mTypes(*this)
    {}

    MemoryModel(const MemoryModel&) = delete;
    MemoryModel& operator=(const MemoryModel&) = delete;

    /// Returns all memory objects found within the given function.
    virtual void findMemoryObjects(
        llvm::Function& function,
        std::vector<MemoryObject*>& objects
    ) = 0;

    /// Translates the given LoadInst into an assignable expression.
    virtual ExprRef<> handleLoad(const llvm::LoadInst& load) = 0;  

    virtual ExprRef<> handleGetElementPtr(const llvm::GEPOperator& gep) = 0;

    virtual std::optional<VariableAssignment> handleStore(
        const llvm::StoreInst& store, ExprPtr pointer, ExprPtr value
    ) = 0;

    virtual gazer::Type& handlePointerType(const llvm::PointerType* type) = 0;
    virtual gazer::Type& handleArrayType(const llvm::ArrayType* type) = 0;

    GazerContext& getContext() { return mContext; }
    gazer::Type& translateType(const llvm::Type* type) { return mTypes.get(type); }

    virtual ~MemoryModel() {}

protected:
    GazerContext& mContext;
    LLVMTypeTranslator mTypes;
};

/// A really simple memory model which represents the whole memory as one
/// undefined memory object. Load operations return an unknown value and
/// store instructions have no effect. No MemoryObjectPhis are inserted.
class DummyMemoryModel : public MemoryModel
{
public:
    using MemoryModel::MemoryModel;

    void findMemoryObjects(
        llvm::Function& function,
        std::vector<MemoryObject*>& objects
    ) override;

    ExprRef<> handleLoad(const llvm::LoadInst& load) override;

    ExprRef<> handleGetElementPtr(const llvm::GEPOperator& gep) override;

    std::optional<VariableAssignment> handleStore(
        const llvm::StoreInst& store, ExprPtr pointer, ExprPtr value
    ) override;
    
    gazer::Type& handlePointerType(const llvm::PointerType* type) override;
    gazer::Type& handleArrayType(const llvm::ArrayType* type) override;
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
