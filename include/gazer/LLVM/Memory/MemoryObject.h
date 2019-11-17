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
#include <llvm/IR/CallSite.h>

#include <boost/iterator/indirect_iterator.hpp>

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
public:
    MemoryAccess(MemoryObject* object)
        : mObject(object)
    {}

    MemoryObject* getObject() const { return mObject; }

private:
    MemoryObject* mObject;
};

/// Represents the definition for a memory object.
class MemoryObjectDef : public MemoryAccess
{
    friend class memory::MemorySSABuilder;
public:
    enum Kind
    {
        LiveOnEntry, ///< Inidicates that the memory object is alive when entering the function.
        GlobalInitializer, ///< One time initialization of a global variable.
        Alloca,  ///< The allocation/instantiation of a local variable.
        Store,  ///< A definition through a store instruction.
        Call,
        Phi
    };

protected:
    MemoryObjectDef(MemoryObject* object, unsigned version, Kind kind)
        : MemoryAccess(object), mKind(kind), mVersion(version)
    {}

public:
    unsigned getVersion() const { return mVersion; }
    Kind getKind() const { return mKind; }
    std::string getName() const;

    void print(llvm::raw_ostream& os) const;

    virtual ~MemoryObjectDef() {}

protected:
    virtual void doPrint(llvm::raw_ostream& os) const = 0;

private:
    Kind mKind;
    unsigned mVersion;
};

class MemoryObjectUse : public MemoryAccess
{
    friend class memory::MemorySSABuilder;
public:
    static unsigned constexpr UnknownVersion = std::numeric_limits<unsigned>::max();

    enum Kind
    {
        Load,   ///< Use through a load instruction.
        Call,
        Return  ///< Indicates that the object is alive at return.
    };

protected:
    MemoryObjectUse(MemoryObject* object, Kind kind)
        : MemoryAccess(object), mKind(kind)
    {}

public:
    MemoryObjectDef* getReachingDef() const { return mReachingDef; }
    Kind getKind() const { return mKind; }

    virtual void print(llvm::raw_ostream& os) const = 0;

    virtual ~MemoryObjectUse() {}

private:
    void setReachingDef(MemoryObjectDef* reachingDef) { mReachingDef = reachingDef; }

private:
    Kind mKind;
    MemoryObjectDef* mReachingDef = nullptr;
};

enum class MemoryObjectType
{
    Unknown = 0,
    Scalar,         ///< a primitive scalar type
    Array,          ///< an array of some known type
    Struct          ///< a struct
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
        llvm::Type* valueType,
        llvm::StringRef name = ""
    ) :
        mId(id), mObjectType(objectType), mSize(size),
        mValueType(valueType), mName(!name.empty() ? name.str() : std::to_string(mId))
    {}

    void print(llvm::raw_ostream& os) const;
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

    ~MemoryObject();

private:
    void addDefinition(MemoryObjectDef* def);
    void addUse(MemoryObjectUse* use);

private:
    unsigned mId;
    MemoryObjectType mObjectType;
    MemoryObjectSize mSize;

    llvm::Type* mValueType;
    std::string mName;

    std::vector<std::unique_ptr<MemoryObjectDef>> mDefs;
    std::vector<std::unique_ptr<MemoryObjectUse>> mUses;
};

inline llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const MemoryObject& memoryObject) {
    memoryObject.print(os);
    return os;
}

namespace memory
{

// Definitions
//===----------------------------------------------------------------------===//

class LiveOnEntryDef : public MemoryObjectDef
{
public:
    LiveOnEntryDef(MemoryObject* object, unsigned int version)
        : MemoryObjectDef(object, version, MemoryObjectDef::LiveOnEntry)
    {}

    void doPrint(llvm::raw_ostream& os) const override;

    static bool classof(const MemoryObjectDef* def) {
        return def->getKind() == MemoryObjectDef::LiveOnEntry;
    }
};

class AllocaDef : public MemoryObjectDef
{
public:
    AllocaDef(MemoryObject* object, unsigned int version, llvm::AllocaInst& alloca)
        : MemoryObjectDef(object, version, MemoryObjectDef::Alloca), mAllocaInst(&alloca)
    {}

    void doPrint(llvm::raw_ostream& os) const override;

    llvm::AllocaInst* getInstruction() const { return mAllocaInst; }

    static bool classof(const MemoryObjectDef* def) {
        return def->getKind() == MemoryObjectDef::Alloca;
    }

private:
    llvm::AllocaInst* mAllocaInst;
};

class GlobalInitializerDef : public MemoryObjectDef
{
public:
    GlobalInitializerDef(MemoryObject* object, unsigned int version, llvm::Value* initValue = nullptr)
        : MemoryObjectDef(object, version, MemoryObjectDef::GlobalInitializer), mInitializer(initValue)
    {}

    llvm::Value* getInitializer() const { return mInitializer; }

    static bool classof(const MemoryObjectDef* def) {
        return def->getKind() == MemoryObjectDef::GlobalInitializer;
    }

protected:
    void doPrint(llvm::raw_ostream& os) const override;
private:
    llvm::Value* mInitializer;
};

class StoreDef : public MemoryObjectDef
{
public:
    StoreDef(MemoryObject* object, unsigned int version, llvm::StoreInst& store)
        : MemoryObjectDef(object, version, MemoryObjectDef::Store),
        mStore(&store)
    {}

    llvm::StoreInst* getStoreInst() { return mStore; }

    void doPrint(llvm::raw_ostream& os) const override;

    static bool classof(const MemoryObjectDef* def) {
        return def->getKind() == MemoryObjectDef::Store;
    }

private:
    llvm::StoreInst* mStore;
};

class CallDef : public MemoryObjectDef
{
public:
    CallDef(MemoryObject* object, unsigned int version, llvm::ImmutableCallSite call)
        : MemoryObjectDef(object, version, MemoryObjectDef::Call), mCall(call)
    {}

    static bool classof(const MemoryObjectDef* def) {
        return def->getKind() == MemoryObjectDef::Call;
    }

protected:
    void doPrint(llvm::raw_ostream& os) const override;

private:
    llvm::ImmutableCallSite mCall;
};

class PhiDef : public MemoryObjectDef
{
    using PhiEntry = std::pair<MemoryObjectDef*, llvm::BasicBlock*>;
public:
    PhiDef(MemoryObject* object, unsigned int version)
        : MemoryObjectDef(object, version, MemoryObjectDef::Phi)
    {}

    void addIncoming(MemoryObjectDef* def, llvm::BasicBlock* bb);

    static bool classof(const MemoryObjectDef* def) {
        return def->getKind() == MemoryObjectDef::Phi;
    }

protected:
    void doPrint(llvm::raw_ostream& os) const override;

private:
    std::vector<PhiEntry> mEntryList;
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

private:
    llvm::LoadInst* mLoadInst;
};

class CallUse : public MemoryObjectUse
{
public:
    CallUse(MemoryObject* object, llvm::CallSite callSite)
        : MemoryObjectUse(object, MemoryObjectUse::Call), mCallSite(callSite)
    {}

    void print(llvm::raw_ostream& os) const override;

private:
    llvm::CallSite mCallSite;
};

} // end namespace gazer::memory

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
