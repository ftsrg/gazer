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

#include "gazer/LLVM/Memory/MemorySSA.h"
#include "gazer/LLVM/Memory/MemoryObject.h"

#include <llvm/Analysis/IteratedDominanceFrontier.h>
#include <llvm/IR/AssemblyAnnotationWriter.h>
#include <llvm/Support/FormattedStream.h>

using namespace gazer;
using gazer::memory::MemorySSA;
using gazer::memory::MemorySSABuilder;

auto MemorySSA::definitionAnnotationsFor(const llvm::Value* value) -> llvm::iterator_range<def_iterator>
{
    auto& range = mValueDefs[value];
    def_iterator begin = boost::make_indirect_iterator(range.begin());
    def_iterator end = boost::make_indirect_iterator(range.end());

    return llvm::make_range(begin, end);
}

auto MemorySSA::useAnnotationsFor(const llvm::Value* value) -> llvm::iterator_range<use_iterator>
{
    auto& range = mValueUses[value];
    use_iterator begin = boost::make_indirect_iterator(range.begin());
    use_iterator end = boost::make_indirect_iterator(range.end());

    return llvm::make_range(begin, end);
}

// Builder
//==------------------------------------------------------------------------==//

MemoryObject* MemorySSABuilder::createMemoryObject(
    MemoryObjectType objectType,
    MemoryObject::MemoryObjectSize size,
    llvm::Type* valueType,
    llvm::StringRef name
) {
    auto& ptr= mObjectStorage.emplace_back(std::make_unique<MemoryObject>(
        mId++, objectType, size, valueType, name
    ));

    return &*ptr;
}

memory::LiveOnEntryDef* MemorySSABuilder::createLiveOnEntry(gazer::MemoryObject* object)
{
    auto def = new memory::LiveOnEntryDef(object, mVersionNumber++);
    mObjectInfo[object].defBlocks.insert(&mFunction.getEntryBlock());
    mValueDefs[&mFunction.getEntryBlock()].push_back(def);
    object->addDefinition(def);

    return def;
}

memory::StoreDef* MemorySSABuilder::createStoreDef(MemoryObject* object, llvm::StoreInst& inst)
{
    auto def = new memory::StoreDef(object, mVersionNumber++, inst);
    mObjectInfo[object].defBlocks.insert(inst.getParent());
    mValueDefs[&inst].push_back(def);
    object->addDefinition(def);

    return def;
}

memory::CallDef* MemorySSABuilder::createCallDef(gazer::MemoryObject* object, llvm::CallSite call)
{
    auto def = new memory::CallDef(object, mVersionNumber++, call);
    mObjectInfo[object].defBlocks.insert(call.getInstruction()->getParent());
    mValueDefs[call.getInstruction()].push_back(def);
    object->addDefinition(def);

    return def;
}

memory::AllocDef* MemorySSABuilder::createAllocDef(
    gazer::MemoryObject* object, gazer::memory::AllocDef::AllocKind allocKind
)
{
    auto def = new memory::AllocDef(object, mVersionNumber++, allocKind);
    object->addDefinition(def);

    return def;
}

memory::LoadUse* MemorySSABuilder::createLoadUse(MemoryObject* object, llvm::LoadInst& load)
{
    auto use = new memory::LoadUse(object, load);
    mValueUses[&load].push_back(use);
    object->addUse(use);

    return use;
}

auto MemorySSABuilder::build() -> std::unique_ptr<MemorySSA>
{
    this->calculatePHINodes();
    this->renamePass();

    return std::unique_ptr<MemorySSA>(new MemorySSA(
        mFunction, mDataLayout, mDominatorTree,
        std::move(mObjectStorage),
        std::move(mValueDefs),
        std::move(mValueUses)
    ));
}

void MemorySSABuilder::calculatePHINodes()
{
    for (auto& object : mObjectStorage) {
        llvm::ForwardIDFCalculator idf(mDominatorTree);
        idf.setDefiningBlocks(mObjectInfo[&*object].defBlocks);

        llvm::SmallVector<llvm::BasicBlock*, 32> phiBlocks;
        idf.calculate(phiBlocks);

        for (auto& bb : phiBlocks) {
            auto phi = new memory::PhiDef(&*object, mVersionNumber++);
            object->addDefinition(phi);
            mValueDefs[bb].push_back(phi);
        }
    }
}

void MemorySSABuilder::renamePass()
{
    llvm::DomTreeNode* root = mDominatorTree.getRootNode();
    renameBlock(root->getBlock());
}

void MemorySSABuilder::renameBlock(llvm::BasicBlock* block)
{
    llvm::SmallDenseMap<MemoryObject*, unsigned, 4> defsInThisBlock;

    // Handle all block-level annotations first
    for (auto& def : mValueDefs[block]) {
        MemoryObject* object = def->getObject();
        mObjectInfo[object].renameStack.push_back(&*def);
        defsInThisBlock[object]++;
    }

    // Handle each instruction in this block
    for (llvm::Instruction& inst : *block) {
        for (auto& use : mValueUses[&inst]) {
            MemoryObject* object = use->getObject();
            MemoryObjectDef* reachingDef = mObjectInfo[object].getCurrentTopDefinition();
            use->setReachingDef(reachingDef);
        }

        for (auto& def : mValueDefs[&inst]) {
            MemoryObject* object = def->getObject();
            mObjectInfo[object].renameStack.push_back(&*def);
            defsInThisBlock[object]++;
        }
    }

    // Handle successor PHIs
    for (llvm::BasicBlock* child : llvm::successors(block)) {
        for (auto& def : mValueDefs[child]) {
            if (auto phi = llvm::dyn_cast<memory::PhiDef>(&*def)) {
                MemoryObject* object = phi->getObject();
                phi->addIncoming(mObjectInfo[object].getCurrentTopDefinition(), block);
            }
        }
    }

    // Handle successors
    for (llvm::DomTreeNode* child : mDominatorTree.getNode(block)->getChildren()) {
        renameBlock(child->getBlock());
    }

    // Finally, unwind the stack
    for (auto& pair : defsInThisBlock) {
        MemoryObject* object = pair.first;
        auto& stack = mObjectInfo[object].renameStack;
        stack.resize(stack.size() - pair.second);
    }
}

// Printing
//==------------------------------------------------------------------------==//

namespace
{

class MemorySSAAnnotatedWriter : public llvm::AssemblyAnnotationWriter
{
public:
    explicit MemorySSAAnnotatedWriter(MemorySSA* memorySSA)
        : mMemorySSA(memorySSA)
    {}

    void emitBasicBlockStartAnnot(const llvm::BasicBlock* block, llvm::formatted_raw_ostream& os) override
    {
        for (MemoryObjectDef& def : mMemorySSA->definitionAnnotationsFor(block)) {
            os << "; ";
            def.print(os);
            os << "\n";
        }

        for (MemoryObjectUse& use : mMemorySSA->useAnnotationsFor(block)) {
            os << "; ";
            use.print(os);
            os << "\n";
        }
    }

    void emitInstructionAnnot(const llvm::Instruction* inst, llvm::formatted_raw_ostream& os) override
    {
        for (MemoryObjectDef& def : mMemorySSA->definitionAnnotationsFor(inst)) {
            os << "; ";
            def.print(os);
            os << "\n";
        }

        for (MemoryObjectUse& use : mMemorySSA->useAnnotationsFor(inst)) {
            os << "; ";
            use.print(os);
            os << "\n";
        }
    }

private:
    MemorySSA* mMemorySSA;
};

}

void MemorySSA::print(llvm::raw_ostream& os)
{
    MemorySSAAnnotatedWriter writer(this);
    mFunction.print(os, &writer);
}