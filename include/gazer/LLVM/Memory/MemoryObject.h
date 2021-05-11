//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#ifndef GAZER_LLVM_MEMORY_MEMORYOBJECT_H
#define GAZER_LLVM_MEMORY_MEMORYOBJECT_H

#include "gazer/Core/Expr.h"
#include "gazer/LLVM/TypeTranslator.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/Operator.h>
#include <llvm/Pass.h>
#include <llvm/IR/ValueHandle.h>
#include <llvm/IR/AbstractCallSite.h>

#include <boost/iterator/indirect_iterator.hpp>

#include <variant>

namespace gazer
{

namespace memory
{
    class MemorySSA;
    class MemorySSABuilder;
}

class MemoryObject;
class MemoryObjectDef;
class MemoryObjectUse;
class MemoryObjectPhi;

class MemoryAccess
{
    friend class memory::MemorySSABuilder;
public:
    MemoryAccess(MemoryObject* object)
        : mObject(object)
    {}

    MemoryAccess(const MemoryAccess&) = delete;
    MemoryAccess& operator=(const MemoryAccess&) = delete;

    MemoryObject* getObject() const { return mObject; }
    MemoryObjectDef* getReachingDef() const { return mReachingDef; }

private:
    void setReachingDef(MemoryObjectDef* reachingDef) { mReachingDef = reachingDef; }

private:
    MemoryObject* mObject;
    MemoryObjectDef* mReachingDef = nullptr;
};

/// Represents the definition for a memory object.
class MemoryObjectDef : public MemoryAccess
{
    friend class memory::MemorySSABuilder;
public:
    enum Kind
    {
        LiveOnEntry,        ///< Inidicates that the memory object is alive when entering the function.
        GlobalInitializer,  ///< One time initialization of a global variable.
        Alloca,             ///< The allocation/instantiation of a local variable.
        Store,              ///< A definition through a store instruction.
        Call,               ///< Indicates that the call possibly clobbers this memory object.
        PHI
    };

protected:
    MemoryObjectDef(MemoryObject* object, unsigned version, Kind kind)
        : MemoryAccess(object), mKind(kind), mVersion(version)
    {}

public:
    unsigned getVersion() const { return mVersion; }
    Kind getKind() const { return mKind; }
    std::string getName() const;
    llvm::BasicBlock* getParentBlock() const;

    void print(llvm::raw_ostream& os) const;

    virtual ~MemoryObjectDef() {}

protected:
    virtual void doPrint(llvm::raw_ostream& os) const = 0;

private:
    Kind mKind;
    unsigned mVersion;
};

inline llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const MemoryObjectDef& def)
{
    def.print(os);
    return os;
}

class MemoryObjectUse : public MemoryAccess
{
public:
    static unsigned constexpr UnknownVersion = std::numeric_limits<unsigned>::max();

    enum Kind
    {
        Load,   ///< Use through a load instruction.
        Call,   ///< Parameter passing into a call.
        Return  ///< Indicates that the object is alive at return.
    };

protected:
    MemoryObjectUse(MemoryObject* object, Kind kind)
        : MemoryAccess(object), mKind(kind)
    {}

public:
    Kind getKind() const { return mKind; }

    llvm::BasicBlock* getParentBlock() const;
    virtual llvm::Instruction* getInstruction() const = 0;
    virtual void print(llvm::raw_ostream& os) const = 0;

    virtual ~MemoryObjectUse() {}

private:
    Kind mKind;
};

/// Describes the type of a memory object. This information will be used by
/// later translation passes to represent the memory object as an SMT formula.
enum class MemoryObjectType
{
    Unknown,
    Scalar,
    Array,
    Struct
};

/// Represents an abstract memory object, a continuous place in memory
/// which does not overlap with other memory objects.
class MemoryObject
{
    friend class memory::MemorySSABuilder;
public:
    using MemoryObjectSize = uint64_t;
    static constexpr MemoryObjectSize UnknownSize = ~uint64_t(0);

public:
    MemoryObject(
        unsigned id,
        MemoryObjectType objectType,
        MemoryObjectSize size,
        llvm::Type* valueType = nullptr,
        llvm::StringRef name = ""
    ) :
        mId(id), mObjectType(objectType), mSize(size),
        mValueType(valueType), mName(!name.empty() ? name.str() : std::to_string(mId))
    {}

    void print(llvm::raw_ostream& os) const;

    /// Sets a type hint for this memory object, indicating that it should be represented
    /// as the given type.
    void setTypeHint(gazer::Type& type) { mTypeHint = &type; }

    /// Returns the type hint if it is set, false otherwise.
    gazer::Type* getTypeHint() const { return mTypeHint; }

    MemoryObjectType getObjectType() const { return mObjectType; }

    llvm::Type* getValueType() const { return mValueType; }
    llvm::StringRef getName() const { return mName; }

    unsigned getId() const { return mId; }

    // Iterator support
    //===------------------------------------------------------------------===//
    using def_iterator = boost::indirect_iterator<std::vector<std::unique_ptr<MemoryObjectDef>>::iterator>;
    def_iterator def_begin() { return boost::make_indirect_iterator(mDefs.begin()); }
    def_iterator def_end() { return boost::make_indirect_iterator(mDefs.end()); }
    llvm::iterator_range<def_iterator> defs() { return llvm::make_range(def_begin(), def_end()); }

    using use_iterator = boost::indirect_iterator<std::vector<std::unique_ptr<MemoryObjectUse>>::iterator>;
    use_iterator use_begin() { return boost::make_indirect_iterator(mUses.begin()); }
    use_iterator use_end() { return boost::make_indirect_iterator(mUses.end()); }
    llvm::iterator_range<use_iterator> uses() { return llvm::make_range(use_begin(), use_end()); }

    MemoryObjectDef* getEntryDef() const { return mEntryDef; }
    MemoryObjectUse* getExitUse() const { return mExitUse; }

    bool hasEntryDef() const { return mEntryDef != nullptr; }
    bool hasExitUse() const { return mExitUse != nullptr; }

    ~MemoryObject();

private:
    void addDefinition(MemoryObjectDef* def);
    void addUse(MemoryObjectUse* use);
    void setEntryDef(MemoryObjectDef* def);
    void setExitUse(MemoryObjectUse* use);

private:
    unsigned mId;
    MemoryObjectType mObjectType;
    MemoryObjectSize mSize;

    gazer::Type* mTypeHint;

    llvm::Type* mValueType;
    std::string mName;

    std::vector<std::unique_ptr<MemoryObjectDef>> mDefs;
    std::vector<std::unique_ptr<MemoryObjectUse>> mUses;
    MemoryObjectDef* mEntryDef = nullptr;
    MemoryObjectUse* mExitUse = nullptr;
};

inline llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const MemoryObject& memoryObject) {
    memoryObject.print(os);
    return os;
}

namespace memory
{

// Definitions
//===----------------------------------------------------------------------===//

class InstructionAnnotationDef : public MemoryObjectDef
{
protected:
    using MemoryObjectDef::MemoryObjectDef;

public:
    virtual llvm::Instruction* getInstruction() const = 0;

    static bool classof(const MemoryObjectDef* def)
    {
        switch (def->getKind()) {
            case LiveOnEntry:
            case GlobalInitializer:
            case PHI:
                return false;
            case Alloca:
            case Store:
            case Call:
                return true;
        }

        llvm_unreachable("Invalid definition kind!");
    }
};

class BlockAnnotationDef : public MemoryObjectDef
{
protected:
    using MemoryObjectDef::MemoryObjectDef;

public:
    virtual llvm::BasicBlock* getBlock() const = 0;

    static bool classof(const MemoryObjectDef* def)
    {
        switch (def->getKind()) {
            case LiveOnEntry:
            case GlobalInitializer:
            case PHI:
                return true;
            case Alloca:
            case Store:
            case Call:
                return false;
        }

        llvm_unreachable("Invalid definition kind!");
    }
};

class AllocaDef : public InstructionAnnotationDef
{
public:
    AllocaDef(MemoryObject* object, unsigned int version, llvm::AllocaInst& alloca)
        : InstructionAnnotationDef(object, version, MemoryObjectDef::Alloca), mAllocaInst(&alloca)
    {}

    void doPrint(llvm::raw_ostream& os) const override;

    llvm::AllocaInst* getInstruction() const override { return mAllocaInst; }

    static bool classof(const MemoryObjectDef* def) {
        return def->getKind() == MemoryObjectDef::Alloca;
    }

private:
    llvm::AllocaInst* mAllocaInst;
};

class StoreDef : public InstructionAnnotationDef
{
public:
    StoreDef(MemoryObject* object, unsigned int version, llvm::StoreInst& store)
        : InstructionAnnotationDef(object, version, MemoryObjectDef::Store),
        mStore(&store)
    {}

    llvm::StoreInst* getInstruction() const override { return mStore; }

    void doPrint(llvm::raw_ostream& os) const override;

    static bool classof(const MemoryObjectDef* def) {
        return def->getKind() == MemoryObjectDef::Store;
    }

private:
    llvm::StoreInst* mStore;
};

class CallDef : public InstructionAnnotationDef
{
public:
    CallDef(MemoryObject* object, unsigned int version, llvm::CallBase* call)
        : InstructionAnnotationDef(object, version, MemoryObjectDef::Call), mCall(call)
    {}

    static bool classof(const MemoryObjectDef* def) {
        return def->getKind() == MemoryObjectDef::Call;
    }

    llvm::Instruction* getInstruction() const override { return mCall; }

protected:
    void doPrint(llvm::raw_ostream& os) const override;

private:
    llvm::CallBase* mCall;
};

class LiveOnEntryDef : public BlockAnnotationDef
{
public:
    LiveOnEntryDef(MemoryObject* object, unsigned int version, llvm::BasicBlock* block)
        : BlockAnnotationDef(object, version, MemoryObjectDef::LiveOnEntry), mEntryBlock(block)
    {}

    void doPrint(llvm::raw_ostream& os) const override;
    llvm::BasicBlock* getBlock() const override { return mEntryBlock; }

    static bool classof(const MemoryObjectDef* def) {
        return def->getKind() == MemoryObjectDef::LiveOnEntry;
    }
private:
    llvm::BasicBlock* mEntryBlock;
};

class GlobalInitializerDef : public BlockAnnotationDef
{
public:
    GlobalInitializerDef(
        MemoryObject* object,
        unsigned int version,
        llvm::BasicBlock* block,
        llvm::GlobalVariable* gv
    )
        : BlockAnnotationDef(object, version, MemoryObjectDef::GlobalInitializer),
        mEntryBlock(block),
        mGlobalVariable(gv)
    {}

    llvm::GlobalVariable* getGlobalVariable() const { return mGlobalVariable; }
    llvm::BasicBlock* getBlock() const override { return mEntryBlock; }

    static bool classof(const MemoryObjectDef* def) {
        return def->getKind() == MemoryObjectDef::GlobalInitializer;
    }

protected:
    void doPrint(llvm::raw_ostream& os) const override;
private:
    llvm::BasicBlock* mEntryBlock;
    llvm::GlobalVariable* mGlobalVariable;
};

class PhiDef : public BlockAnnotationDef
{
public:
    PhiDef(MemoryObject* object, unsigned int version, llvm::BasicBlock* block)
        : BlockAnnotationDef(object, version, MemoryObjectDef::PHI),
        mBlock(block)
    {}

    llvm::BasicBlock* getBlock() const override { return mBlock; }

    MemoryObjectDef* getIncomingDefForBlock(const llvm::BasicBlock* bb) const;
    void addIncoming(MemoryObjectDef* def, const llvm::BasicBlock* bb);

    static bool classof(const MemoryObjectDef* def) {
        return def->getKind() == MemoryObjectDef::PHI;
    }

protected:
    void doPrint(llvm::raw_ostream& os) const override;

private:
    llvm::BasicBlock* mBlock;
    llvm::DenseMap<const llvm::BasicBlock*, MemoryObjectDef*> mEntryList;
};

// Uses
//===----------------------------------------------------------------------===//

class LoadUse : public MemoryObjectUse
{
public:
    LoadUse(MemoryObject* object, llvm::LoadInst& load)
        : MemoryObjectUse(object, MemoryObjectUse::Load), mLoadInst(&load)
    {}

    void print(llvm::raw_ostream& os) const override;

    llvm::LoadInst* getInstruction() const override { return mLoadInst; }

    static bool classof(const MemoryObjectUse* use) {
        return use->getKind() == MemoryObjectUse::Load;
    }
private:
    llvm::LoadInst* mLoadInst;
};

class CallUse : public MemoryObjectUse
{
public:
    CallUse(MemoryObject* object, llvm::CallBase* callSite)
        : MemoryObjectUse(object, MemoryObjectUse::Call), mCallSite(callSite)
    {}

    llvm::Instruction* getInstruction() const override { return mCallSite; }
    llvm::CallBase* getCallSite() const { return mCallSite; }

    void print(llvm::raw_ostream& os) const override;

    static bool classof(const MemoryObjectUse* use) {
        return use->getKind() == MemoryObjectUse::Call;
    }
private:
    llvm::CallBase* mCallSite;
};

class RetUse : public MemoryObjectUse
{
public:
    RetUse(MemoryObject* object, llvm::ReturnInst& ret)
        : MemoryObjectUse(object, MemoryObjectUse::Return), mReturnInst(&ret)
    {}

    llvm::ReturnInst* getInstruction() const override { return mReturnInst; }
    void print(llvm::raw_ostream& os) const override;

    static bool classof(const MemoryObjectUse* use) {
        return use->getKind() == MemoryObjectUse::Return;
    }
private:
    llvm::ReturnInst* mReturnInst;
};

} // end namespace gazer::memory

} // end namespace gazer

#endif
