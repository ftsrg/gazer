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
#ifndef GAZER_LLVM_MEMORY_MEMSSA_H
#define GAZER_LLVM_MEMORY_MEMSSA_H

#include "gazer/LLVM/Memory/MemoryObject.h"

#include <boost/iterator/indirect_iterator.hpp>

#include <unordered_map>

namespace gazer::memory
{

class MemorySSABuilder;

class MemorySSA
{
    friend class MemorySSABuilder;

    using ValueToDefSetMap = llvm::DenseMap<const llvm::Value*, std::vector<MemoryObjectDef*>>;
    using ValueToUseSetMap = llvm::DenseMap<const llvm::Value*, std::vector<MemoryObjectUse*>>;
private:
    MemorySSA(
        llvm::Function& function,
        std::vector<std::unique_ptr<MemoryObject>> objects,
        ValueToDefSetMap valueDefAnnotations,
        ValueToUseSetMap valueUseAnnotations
    ) : mFunction(function),
        mObjects(std::move(objects)),
        mValueDefs(std::move(valueDefAnnotations)),
        mValueUses(std::move(valueUseAnnotations))
    {}

public:
    void print(llvm::raw_ostream& os);

    // Def-use queries
    //==--------------------------------------------------------------------==//
    using def_iterator = boost::indirect_iterator<ValueToDefSetMap::mapped_type::iterator>;
    using use_iterator = boost::indirect_iterator<ValueToUseSetMap::mapped_type::iterator>;

    llvm::iterator_range<def_iterator> definitionAnnotationsFor(const llvm::Value*);
    llvm::iterator_range<use_iterator> useAnnotationsFor(const llvm::Value*);

    template<class AccessKind>
    typename std::enable_if_t<std::is_base_of_v<MemoryObjectDef, AccessKind>>
    memoryAccessOfKind(const llvm::Value* value, llvm::SmallVectorImpl<AccessKind*>& vec)
    {
        return this->memoryAccessOfKindImpl(this->definitionAnnotationsFor(value), vec);
    }

    template<class AccessKind>
    typename std::enable_if_t<std::is_base_of_v<MemoryObjectUse, AccessKind>>
    memoryAccessOfKind(const llvm::Value* value, llvm::SmallVectorImpl<AccessKind*>& vec)
    {
        return this->memoryAccessOfKindImpl(this->useAnnotationsFor(value), vec);
    }

    /// If \p value is annotated with a single definition for \p object, returns
    /// said definition. In any other case, returns nullptr.
    MemoryObjectDef* getUniqueDefinitionFor(const llvm::Value* value, const MemoryObject* object);

    MemoryObjectUse* getUniqueUseFor(const llvm::Value* value, const MemoryObject* object);

    // Iterator support
    //==--------------------------------------------------------------------==//
    using object_iterator = boost::indirect_iterator<std::vector<std::unique_ptr<MemoryObject>>::iterator>;
    object_iterator object_begin() { return boost::make_indirect_iterator(mObjects.begin()); }
    object_iterator object_end() { return boost::make_indirect_iterator(mObjects.end()); }
    llvm::iterator_range<object_iterator> objects() { return llvm::make_range(object_begin(), object_end()); }

private:

    template<class AccessKind, class Range>
    static void memoryAccessOfKindImpl(const Range& range, llvm::SmallVectorImpl<AccessKind*>& vec)
    {
        static_assert(std::is_base_of_v<MemoryAccess, AccessKind>, "AccessKind must be a subclass of MemoryAccess!");
        for (auto& access : range) {
            if (auto casted = llvm::dyn_cast<AccessKind>(&access)) {
                vec.push_back(casted);
            }
        }
    }

private:
    llvm::Function& mFunction;
    std::vector<std::unique_ptr<MemoryObject>> mObjects;

    // MemorySSA annotations
    ValueToDefSetMap mValueDefs;
    ValueToUseSetMap mValueUses;
};

/// Helper class for building memory SSA form.
class MemorySSABuilder
{
    struct MemoryObjectInfo
    {
        MemoryObject* object;
        llvm::SmallPtrSet<llvm::BasicBlock*, 32> defBlocks;
        llvm::SmallVector<MemoryObjectDef*, 8> renameStack;

        MemoryObjectDef* getCurrentTopDefinition() {
            if (!renameStack.empty()) { return renameStack.back(); }
            return nullptr;
        }
    };
public:
    MemorySSABuilder(
        llvm::Function& function,
        const llvm::DataLayout& dl,
        llvm::DominatorTree& dominatorTree
    )
        : mFunction(function), mDataLayout(dl), mDominatorTree(dominatorTree)
    {}

    MemoryObject* createMemoryObject(
        unsigned id,
        MemoryObjectType objectType,
        MemoryObject::MemoryObjectSize size,
        llvm::Type* valueType,
        llvm::StringRef name = ""
    );

    memory::LiveOnEntryDef* createLiveOnEntryDef(MemoryObject* object);
    memory::GlobalInitializerDef* createGlobalInitializerDef(
        MemoryObject* object, llvm::GlobalVariable* gv
    );
    memory::AllocaDef* createAllocaDef(MemoryObject* object, llvm::AllocaInst& alloca);
    memory::StoreDef* createStoreDef(MemoryObject* object, llvm::StoreInst& inst);
    memory::CallDef*  createCallDef(MemoryObject* object, llvm::CallBase* call);

    memory::LoadUse* createLoadUse(MemoryObject* object, llvm::LoadInst& load);
    memory::CallUse* createCallUse(MemoryObject* object, llvm::CallBase* call);
    memory::RetUse* createReturnUse(MemoryObject* object, llvm::ReturnInst& ret);

    std::unique_ptr<MemorySSA> build();

private:
    void calculatePHINodes();
    void renamePass();
    void renameBlock(llvm::BasicBlock* block);

private:
    llvm::Function& mFunction;
    const llvm::DataLayout& mDataLayout;
    llvm::DominatorTree& mDominatorTree;

    std::vector<std::unique_ptr<MemoryObject>> mObjectStorage;
    llvm::DenseMap<MemoryObject*, MemoryObjectInfo> mObjectInfo;
    MemorySSA::ValueToDefSetMap mValueDefs;
    MemorySSA::ValueToUseSetMap mValueUses;

    unsigned mVersionNumber = 0;
};

/// A wrapper for passes which want to construct memory SSA.
/// Clients can register required analyses
class MemorySSABuilderPass : public llvm::ModulePass
{
public:
    explicit MemorySSABuilderPass(char& ID)
        : ModulePass(ID)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const final;
    bool runOnModule(llvm::Module& llvmModule) final;

    virtual void addRequiredAnalyses(llvm::AnalysisUsage& au) const {}
    virtual void initializeFunction(llvm::Function& function, MemorySSABuilder& builder) = 0;
private:
    llvm::DenseMap<llvm::Function*, std::unique_ptr<MemorySSA>> mFunctions;
};

}

#endif
