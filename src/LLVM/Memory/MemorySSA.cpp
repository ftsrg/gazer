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
using gazer::memory::MemorySSABuilderPass;

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

auto MemorySSA::getUniqueDefinitionFor(const llvm::Value* value, const MemoryObject* object)
    -> MemoryObjectDef*
{
    auto range = this->definitionAnnotationsFor(value);
    MemoryObjectDef* result = nullptr;

    for (MemoryObjectDef& def : range) {
        if (def.getObject() == object) {
            if (result != nullptr) {
                // We have already found a proper object, thus there is no unique definition.
                return nullptr;
            }

            result = &def;
        }
    }

    return result;
}

auto MemorySSA::getUniqueUseFor(const llvm::Value* value, const MemoryObject* object)
    -> MemoryObjectUse*
{
    auto range = this->useAnnotationsFor(value);
    MemoryObjectUse* result = nullptr;

    for (MemoryObjectUse& use : range) {
        if (use.getObject() == object) {
            if (result != nullptr) {
                // We have already found a proper object, thus there is no unique definition.
                return nullptr;
            }

            result = &use;
        }
    }

    return result;
}

// Pass implementation
//==------------------------------------------------------------------------==//
void MemorySSABuilderPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.setPreservesAll();
    au.addRequired<llvm::DominatorTreeWrapperPass>();
    this->addRequiredAnalyses(au);
}

auto MemorySSABuilderPass::runOnModule(llvm::Module& llvmModule) -> bool
{
    for (llvm::Function& function : llvmModule) {
        if (function.isDeclaration()) {
            continue;
        }

        auto& dt = getAnalysis<llvm::DominatorTreeWrapperPass>(function).getDomTree();
        MemorySSABuilder builder(function, llvmModule.getDataLayout(), dt);

        this->initializeFunction(function, builder);

        auto memSSA = builder.build();
        mFunctions.try_emplace(&function, std::move(memSSA));
    }

    return false;
}

// Builder
//==------------------------------------------------------------------------==//

MemoryObject* MemorySSABuilder::createMemoryObject(
    unsigned id,
    MemoryObjectType objectType,
    MemoryObject::MemoryObjectSize size,
    llvm::Type* valueType,
    llvm::StringRef name
) {
    auto& ptr= mObjectStorage.emplace_back(std::make_unique<MemoryObject>(
        id, objectType, size, valueType, name
    ));

    return &*ptr;
}

memory::LiveOnEntryDef* MemorySSABuilder::createLiveOnEntryDef(gazer::MemoryObject* object)
{
    assert(!object->hasEntryDef() && "Attempting to insert two entry definitions for a single object!");

    llvm::BasicBlock* entryBlock = &mFunction.getEntryBlock();
    auto def = new memory::LiveOnEntryDef(object, mVersionNumber++, entryBlock);
    mObjectInfo[object].defBlocks.insert(entryBlock);

    mValueDefs[&mFunction.getEntryBlock()].emplace_back(def);


    object->addDefinition(def);
    object->setEntryDef(def);

    return def;
}

memory::GlobalInitializerDef* MemorySSABuilder::createGlobalInitializerDef(
    gazer::MemoryObject* object, llvm::GlobalVariable* gv)
{
    llvm::BasicBlock* entryBlock = &mFunction.getEntryBlock();
    auto def = new memory::GlobalInitializerDef(object, mVersionNumber++, entryBlock, gv);
    mObjectInfo[object].defBlocks.insert(entryBlock);

    mValueDefs[&mFunction.getEntryBlock()].emplace_back(def);
    
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

memory::CallDef* MemorySSABuilder::createCallDef(gazer::MemoryObject* object, llvm::CallBase* call)
{
    auto def = new memory::CallDef(object, mVersionNumber++, call);
    mObjectInfo[object].defBlocks.insert(call->getParent());
    mValueDefs[call].push_back(def);
    object->addDefinition(def);

    return def;
}

memory::AllocaDef* MemorySSABuilder::createAllocaDef(
    MemoryObject* object, llvm::AllocaInst& alloca
)
{
    auto def = new memory::AllocaDef(object, mVersionNumber++, alloca);
    mObjectInfo[object].defBlocks.insert(alloca.getParent());
    mValueDefs[&alloca].push_back(def);
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

memory::CallUse* MemorySSABuilder::createCallUse(MemoryObject* object, llvm::CallBase* call)
{
    auto use = new memory::CallUse(object, call);
    mValueUses[call].push_back(use);
    object->addUse(use);

    return use;
}

memory::RetUse* MemorySSABuilder::createReturnUse(MemoryObject* object, llvm::ReturnInst& ret)
{
    assert(!object->hasExitUse() && "Attempting to add a duplicate exit use!");
    auto use = new memory::RetUse(object, ret);
    mValueUses[&ret].push_back(use);
    object->addUse(use);
    object->setExitUse(use);

    return use;
}

auto MemorySSABuilder::build() -> std::unique_ptr<MemorySSA>
{
    this->calculatePHINodes();
    this->renamePass();

    return std::unique_ptr<MemorySSA>(new MemorySSA(
        mFunction, std::move(mObjectStorage), std::move(mValueDefs), std::move(mValueUses)
    ));
}

void MemorySSABuilder::calculatePHINodes()
{
    for (auto& object : mObjectStorage) {
        llvm::ForwardIDFCalculator idf(mDominatorTree);
        idf.setDefiningBlocks(mObjectInfo[&*object].defBlocks);

        llvm::SmallVector<llvm::BasicBlock*, 32> phiBlocks;
        idf.calculate(phiBlocks);

        for (llvm::BasicBlock* bb : phiBlocks) {
            auto phi = new memory::PhiDef(&*object, mVersionNumber++, bb);
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

        MemoryObjectDef* reachingDef = mObjectInfo[object].getCurrentTopDefinition();
        if (reachingDef != nullptr) {
            def->setReachingDef(reachingDef);
        }

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

            // Set the previously reaching definition for this access
            MemoryObjectDef* reachingDef = mObjectInfo[object].getCurrentTopDefinition();
            def->setReachingDef(reachingDef);

            // Push the new definition to the top of the renaming stack
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
    for (llvm::DomTreeNode* child : mDominatorTree.getNode(block)->children()) {
        renameBlock(child->getBlock());
    }

    // Finally, unwind the stacks
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

    void emitFunctionAnnot(const llvm::Function* function, llvm::formatted_raw_ostream& os) override
    {
        os << "; Declared memory objects:\n";
        for (MemoryObject& object : mMemorySSA->objects()) {
            os << ";" << object << "\n";
        }
    }

    void emitBasicBlockStartAnnot(const llvm::BasicBlock* block, llvm::formatted_raw_ostream& os) override
    {
        for (MemoryObjectUse& use : mMemorySSA->useAnnotationsFor(block)) {
            os << "; ";
            use.print(os);
            os << "\n";
        }

        for (MemoryObjectDef& def : mMemorySSA->definitionAnnotationsFor(block)) {
            os << "; ";
            def.print(os);
            os << "\n";
        }
    }

    void emitInstructionAnnot(const llvm::Instruction* inst, llvm::formatted_raw_ostream& os) override
    {
        for (MemoryObjectUse& use : mMemorySSA->useAnnotationsFor(inst)) {
            os << "; ";
            use.print(os);
            os << "\n";
        }

        for (MemoryObjectDef& def : mMemorySSA->definitionAnnotationsFor(inst)) {
            os << "; ";
            def.print(os);
            os << "\n";
        }
    }

private:
    MemorySSA* mMemorySSA;
};

} // namespace

void MemorySSA::print(llvm::raw_ostream& os)
{
    MemorySSAAnnotatedWriter writer(this);
    mFunction.print(os, &writer);
}
