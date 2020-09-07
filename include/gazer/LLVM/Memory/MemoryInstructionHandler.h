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
///
/// \file This file declares the MemoryInstructionHandler interface, which
/// will be used by the LLVM IR translation process to represent memory
/// instructions.
///
//===----------------------------------------------------------------------===//
#ifndef GAZER_LLVM_MEMORY_MEMORYINSTRUCTIONHANDLER_H
#define GAZER_LLVM_MEMORY_MEMORYINSTRUCTIONHANDLER_H

#include "gazer/LLVM/Automaton/ModuleToAutomata.h"

namespace gazer
{

/// An interface which allows the translation of pointer and array types.
class MemoryTypeTranslator
{
public:
    explicit MemoryTypeTranslator(GazerContext& context)
        : mContext(context)
    {}

    /// Returns the representation of a given pointer type.
    virtual gazer::Type& handlePointerType(const llvm::PointerType* type) = 0;

    /// Translates types of constant arrays and initializers.
    virtual gazer::Type& handleArrayType(const llvm::ArrayType* type) = 0;

    virtual ~MemoryTypeTranslator() = default;

    GazerContext& getContext() const { return mContext; }

protected:
    GazerContext& mContext;
};

/// An interface for translating memory-related instructions.
class MemoryInstructionHandler
{
public:
    // Variables
    //==--------------------------------------------------------------------==//

    /// Allows the declaration of global variables. Called once per module.
    virtual void declareGlobalVariables(llvm::Module& module,
                                        llvm2cfa::GlobalVarDeclExtensionPoint& ep) {}

    /// Allows the declaration of top-level procedure variables.
    virtual void declareFunctionVariables(llvm2cfa::VariableDeclExtensionPoint& ep) {}

    /// Allows the declaration of loop-procedure level variables.
    virtual void declareLoopProcedureVariables(
        llvm::Loop* loop, llvm2cfa::LoopVarDeclExtensionPoint& ep) {}

    // Allocations
    //==--------------------------------------------------------------------==//

    /// Translates an alloca instruction into an assignable expression.
    virtual ExprPtr handleAlloca(
        const llvm::AllocaInst& alloc,
        llvm2cfa::GenerationStepExtensionPoint& ep) = 0;

    // Pointer value representation
    //==--------------------------------------------------------------------==//
    // These methods are used to translate various pointer-related values.
    // As they are supposed to return expressions without side-effects, these
    // do not get access to an extension point. Instead, they receive the
    // already-translated operands of their instruction as an input parameter.

    /// Represents a given non-instruction pointer.
    virtual ExprPtr handlePointerValue(const llvm::Value* value) = 0;

    /// Represents a pointer casting instruction.
    virtual ExprPtr handlePointerCast(
        const llvm::CastInst& cast,
        const ExprPtr& origPtr) = 0;

    /// Represents a pointer arithmetic instruction.
    virtual ExprPtr handleGetElementPtr(
        const llvm::GetElementPtrInst& gep,
        llvm::ArrayRef<ExprPtr> ops) = 0;

    /// Translates constant arrays.
    virtual ExprPtr handleConstantDataArray(
        const llvm::ConstantDataArray* cda, llvm::ArrayRef<ExprRef<LiteralExpr>> elems) = 0;


    // Memory definitions and uses
    //==--------------------------------------------------------------------==//

    /// Translates the given store instruction.
    virtual void handleStore(
        const llvm::StoreInst& store,
        llvm2cfa::GenerationStepExtensionPoint& ep) = 0;

    /// Handles possible memory definitions in the beginning of blocks.
    virtual void handleBlock(
        const llvm::BasicBlock& bb, llvm2cfa::GenerationStepExtensionPoint& ep)
    {}

    /// Handles possible memory phi-nodes on basic block edges.
    virtual void handleBasicBlockEdge(
        const llvm::BasicBlock& source,
        const llvm::BasicBlock& target,
        llvm2cfa::GenerationStepExtensionPoint& ep)
    {}

    /// Returns the value of a load as an assignable expression.
    virtual ExprPtr handleLoad(
        const llvm::LoadInst& load,
        llvm2cfa::GenerationStepExtensionPoint& ep) = 0;

    /// Translates a given call instruction. Clients can assume that the callee
    /// function has a definition and a corresponding automaton already exists.
    /// The parameters \p inputAssignments and \p outputAssignments will be placed
    /// on the resulting automaton call _after_ the regular input/output assignments.
    virtual void handleCall(
        llvm::CallSite call,
        llvm2cfa::GenerationStepExtensionPoint& callerEp,
        llvm2cfa::AutomatonInterfaceExtensionPoint& calleeEp,
        llvm::SmallVectorImpl<VariableAssignment>& inputAssignments,
        llvm::SmallVectorImpl<VariableAssignment>& outputAssignments) = 0;

    /// If the memory model wishes to handle external calls to unknown functions, it
    /// may do so through this method. Note that known memory-related functions such
    /// as malloc, memset, memcpy, etc. have their own overridable methods, therefore
    /// they should not be handled here. Furthermore, if the call is to a non-void
    /// function, the translation process already generates a havoc assignment for
    /// it _before_ calling this function.
    virtual void handleExternalCall(
        llvm::CallSite call, llvm2cfa::GenerationStepExtensionPoint& ep) {}

    // Memory safety predicates
    //==--------------------------------------------------------------------==//
    
    /// Returns a boolean expression which evaluates to true if a memory access
    /// through \p ptr (represented by the expression in \p expr) is valid.
    virtual ExprPtr isValidAccess(llvm::Value* ptr, const ExprPtr& expr) = 0;

    virtual ~MemoryInstructionHandler() = default;
};

namespace memory
{
class MemorySSA;
}

/// A base class for MemorySSA-based instruction handlers.
class MemorySSABasedInstructionHandler : public MemoryInstructionHandler
{
public:
    MemorySSABasedInstructionHandler(memory::MemorySSA& memorySSA, LLVMTypeTranslator& types)
        : mMemorySSA(memorySSA), mTypes(types)
    {}

    void declareFunctionVariables(llvm2cfa::VariableDeclExtensionPoint& ep) override;    
    void declareLoopProcedureVariables(
        llvm::Loop* loop, llvm2cfa::LoopVarDeclExtensionPoint& ep) override;

    void handleBasicBlockEdge(
        const llvm::BasicBlock& source,
        const llvm::BasicBlock& target,
        llvm2cfa::GenerationStepExtensionPoint& ep) override;

protected:
    virtual gazer::Type& getMemoryObjectType(MemoryObject* object);

protected:
    memory::MemorySSA& mMemorySSA;
    LLVMTypeTranslator& mTypes;
};

} // namespace gazer


#endif
