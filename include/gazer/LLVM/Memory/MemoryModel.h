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
#ifndef GAZER_MEMORY_MEMORYMODEL_H
#define GAZER_MEMORY_MEMORYMODEL_H

#include "gazer/LLVM/Memory/MemorySSA.h"

#include <llvm/Analysis/PtrUseVisitor.h>

namespace llvm
{
    class Loop;
} // end namespace llvm

namespace gazer
{

class Cfa;

class MemoryModel
{
public:
    MemoryModel(
        GazerContext& context,
        LLVMFrontendSettings settings,
        const llvm::DataLayout& dl
    )
        : mContext(context),
        mTypes(*this, settings.ints),
        mDataLayout(dl)
    {}

    MemoryModel(const MemoryModel&) = delete;
    MemoryModel& operator=(const MemoryModel&) = delete;

    /// Initializes this memory model for a specific module.
    /// \param module The LLVM module to initialize this memory model for.
    /// \param getDomTree A callable which returns the dominator tree for each function.
    void initialize(llvm::Module& module, std::function<llvm::DominatorTree&(llvm::Function&)> getDomTree);

    /// Declares all input/output/local variables that should be inserted into \p cfa.
    virtual void declareProcedureVariables(Cfa& cfa, llvm::Function& function) = 0;
    virtual void declareProcedureVariables(Cfa& cfa, llvm::Loop* loop) = 0;

    /// Translates the given LoadInst into an assignable expression.
    virtual ExprPtr handleLoad(const llvm::LoadInst& load) = 0;
    virtual ExprPtr handleGetElementPtr(const llvm::GEPOperator& gep) = 0;
    virtual ExprPtr handleAlloca(const llvm::AllocaInst& alloc) = 0;
    virtual ExprPtr handlePointerCast(const llvm::CastInst& cast) = 0;
    virtual ExprPtr handlePointerValue(const llvm::Value* value) = 0;

    /// Returns an optional variable assignment to represent a Store instruction.
    virtual std::optional<VariableAssignment> handleStore(
        const llvm::StoreInst& store, ExprPtr pointer, ExprPtr value
    ) = 0;

    virtual gazer::Type& handlePointerType(const llvm::PointerType* type) = 0;
    virtual gazer::Type& handleArrayType(const llvm::ArrayType* type) = 0;

    GazerContext& getContext() { return mContext; }
    const llvm::DataLayout& getDataLayout() const { return mDataLayout; }

    gazer::Type& translateType(const llvm::Type* type) { return mTypes.get(type); }
    memory::MemorySSA& getFunctionMemorySSA(llvm::Function& function) {
        return *mFunctions[&function];
    }

    virtual void dump() const {};

    virtual ~MemoryModel() {}

protected:
    /// Fills \p builder with all memory objects, their (possible) definitons and uses.
    virtual void initializeFunction(llvm::Function& function, memory::MemorySSABuilder& builder) = 0;

protected:
    GazerContext& mContext;
    LLVMTypeTranslator mTypes;
    const llvm::DataLayout& mDataLayout;

private:
    std::unordered_map<llvm::Function*, std::unique_ptr<memory::MemorySSA>> mFunctions;
};

class CollectMemoryDefsUsesVisitor : public llvm::PtrUseVisitor<CollectMemoryDefsUsesVisitor>
{
    friend class llvm::PtrUseVisitor<CollectMemoryDefsUsesVisitor>;
    friend class llvm::InstVisitor<CollectMemoryDefsUsesVisitor>;
public:
    explicit CollectMemoryDefsUsesVisitor(const llvm::DataLayout& dl, MemoryObject* obj, memory::MemorySSABuilder& builder)
        : PtrUseVisitor(dl), mObject(obj), mBuilder(builder)
    {}

private:
    void visitStoreInst(llvm::StoreInst& store);
    void visitLoadInst(llvm::LoadInst& load);

    void visitCallInst(llvm::CallInst& call);
    void visitPHINode(llvm::PHINode& phi);
    void visitSelectInst(llvm::SelectInst& select);
    void visitInstruction(llvm::Instruction& inst);

private:
    MemoryObject* mObject;
    memory::MemorySSABuilder& mBuilder;
};

//==-----------------------------------------------------------------------==//
/// DummyMemoryModel - A havoc memory model which does not create any memory
/// objects. oad operations return an unknown value and store instructions
/// have no effect. No MemoryObjectPhis are inserted.
std::unique_ptr<MemoryModel> CreateHavocMemoryModel(
    GazerContext& context,
    LLVMFrontendSettings& settings,
    const llvm::DataLayout& dl
);

//==-----------------------------------------------------------------------==//
// BasicMemoryModel - a simple memory model which handles local arrays,
// structs, and globals which do not have their address taken. This memory
// model returns undef for all heap operations.
std::unique_ptr<MemoryModel> CreateBasicMemoryModel(
    GazerContext& context,
    const LLVMFrontendSettings& settings,
    const llvm::DataLayout& dl
);

}
#endif //GAZER_MEMORY_MEMORYMODEL_H
