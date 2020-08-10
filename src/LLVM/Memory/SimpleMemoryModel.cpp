//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2020 Contributors to the Gazer project
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
/// \file This file defines the SimpleMemoryModel class, a memory model which
/// represents all memory as separate locals or globals.
/// Sound when these cannot alias.
///
//===----------------------------------------------------------------------===//

#include "gazer/LLVM/Memory/MemoryModel.h"

#include <gazer/Core/Expr/ExprBuilder.h>

using namespace gazer;

namespace {

class SimpleMemoryInstructionHandler : public MemoryInstructionHandler, public MemoryTypeTranslator {
    LLVMTypeTranslator types;
    llvm::Module& module;
public:

    SimpleMemoryInstructionHandler(GazerContext& context,
                                   const LLVMFrontendSettings& settings,
                                   llvm::Module& module) :
        MemoryTypeTranslator(context), types(*this, settings), module(module) {
    }

    void declareGlobalVariables(llvm2cfa::GlobalVariableDeclExtensionPoint &ep) override {
        for (const auto& val : module.globals()) {
            auto var = ep.createGlobalVar(&val, types.get(val.getValueType()));
            vars.insert({&val, var});
        }
    }

    /// Allows the declaration of top-level procedure variables.
    void declareFunctionVariables(llvm2cfa::VariableDeclExtensionPoint& ep) override {}

    // Allocations
    //==--------------------------------------------------------------------==//

    llvm::DenseMap<const llvm::Value*, Variable*> vars;

    /// Translates an alloca instruction into an assignable expression.
    ExprPtr handleAlloca(
        const llvm::AllocaInst& alloc,
        llvm2cfa::GenerationStepExtensionPoint& ep) override {
        auto var = ep.createAuxiliaryVariable(alloc.getName(), types.get(alloc.getAllocatedType()));
        vars.insert({&alloc, var});
        return var->getRefExpr();
    }

    // Pointer value representation
    //==--------------------------------------------------------------------==//
    // These methods are used to translate various pointer-related values.
    // As they are supposed to return expressions without side-effects, these
    // do not get access to an extension point. Instead, they receive the
    // already-translated operands of their instruction as an input parameter.

    /// Represents a given non-instruction pointer.
    ExprPtr handlePointerValue(const llvm::Value* value) override {
        llvm_unreachable("Pointers as values are not supported");
        //return ExprBuilder(getContext()).Undef(value->getType());
    }

    /// Represents a pointer casting instruction.
    ExprPtr handlePointerCast(
        const llvm::CastInst& cast,
        const ExprPtr& origPtr) override {
        // TODO this is a fake implementation
        // mostly for the gazer intrinsics that use pointer casts
        // if a real cast would take place, this would make no sense at all.
        // probably the intrinsics themselves would brake if tracing would work...
        auto& goalType = types.get(cast.getDestTy());
        if (cast.getDestTy()->isPointerTy()) {
            return ExprBuilder(getContext()).BvLit8(0);
        }
        switch(goalType.getTypeID()) {
            case gazer::Type::BoolTypeID: return ExprBuilder(getContext()).False();
            case gazer::Type::IntTypeID: return ExprBuilder(getContext()).IntLit(0);
            case gazer::Type::BvTypeID: return ExprBuilder(getContext()).BvLit(0, llvm::cast<BvType>(goalType).getWidth());
        }
        llvm_unreachable("Unsupported pointer cast");
    }

    /// Represents a pointer arithmetic instruction.
    ExprPtr handleGetElementPtr(
        const llvm::GetElementPtrInst& gep,
        llvm::ArrayRef<ExprPtr> ops) override {
        llvm_unreachable("GEP (probably lowered from array accesses) are not supported");
    }

    /// Translates constant arrays.
    ExprPtr handleConstantDataArray(
        const llvm::ConstantDataArray* cda, llvm::ArrayRef<ExprRef<LiteralExpr>> elems) override {
        llvm_unreachable("CDA (probably lowered from constant array accesses) are not supported");
    }

    // Memory definitions and uses
    //==--------------------------------------------------------------------==//

    /// Translates the given store instruction.
    void handleStore(
        const llvm::StoreInst& store,
        llvm2cfa::GenerationStepExtensionPoint& ep) override {
        ep.insertAssignment(vars[store.getPointerOperand()], ep.getAsOperand(store.getValueOperand()));
    }

    /// Handles possible memory definitions in the beginning of blocks.
    void handleBlock(
        const llvm::BasicBlock& bb, llvm2cfa::GenerationStepExtensionPoint& ep) override
    {}

    /// Handles possible memory phi-nodes on basic block edges.
    void handleBasicBlockEdge(
        const llvm::BasicBlock& source,
        const llvm::BasicBlock& target,
        llvm2cfa::GenerationStepExtensionPoint& ep) override
    {}

    /// Returns the value of a load as an assignable expression.
    ExprPtr handleLoad(
        const llvm::LoadInst& load,
        llvm2cfa::GenerationStepExtensionPoint& ep) override {
        auto var = vars[load.getPointerOperand()];
        // save the current value of the variable
        // TODO this can be done smarter if there is no store after this particular load
        auto resultVar = ep.createAuxiliaryVariable("load_" + var->getName(), var->getType());
        ep.insertAssignment(resultVar, var->getRefExpr());
        return resultVar->getRefExpr();
    }

    /// Translates a given call instruction. Clients can assume that the callee
    /// function has a definition and a corresponding automaton already exists.
    /// The parameters \p inputAssignments and \p outputAssignments will be placed
    /// on the resulting automaton call _after_ the regular input/output assignments.
    void handleCall(
        llvm::CallSite call,
        llvm2cfa::GenerationStepExtensionPoint& callerEp,
        llvm2cfa::AutomatonInterfaceExtensionPoint& calleeEp,
        llvm::SmallVectorImpl<VariableAssignment>& inputAssignments,
        llvm::SmallVectorImpl<VariableAssignment>& outputAssignments) override {

    }

    /// If the memory model wishes to handle external calls to unknown functions, it
    /// may do so through this method. Note that known memory-related functions such
    /// as malloc, memset, memcpy, etc. have their own overridable methods, therefore
    /// they should not be handled here. Furthermore, if the call is to a non-void
    /// function, the translation process already generates a havoc assignment for
    /// it _before_ calling this function.
    void handleExternalCall(
        llvm::CallSite call, llvm2cfa::GenerationStepExtensionPoint& ep) override {}

    // Memory safety predicates
    //==--------------------------------------------------------------------==//

    /// Returns a boolean expression which evaluates to true if a memory access
    /// through \p ptr (represented by the expression in \p expr) is valid.
    ExprPtr isValidAccess(llvm::Value* ptr, const ExprPtr& expr) override {
        // TODO
        return ExprBuilder(getContext()).True();
    }

    ~SimpleMemoryInstructionHandler() override = default;

    gazer::Type& handlePointerType(const llvm::PointerType* type) override {
        // TODO this is a fake implementation, and I'm not quite sure what are the dependencies
        return ExprBuilder(getContext()).BvLit8(0)->getType();
    }

    /// Translates types of constant arrays and initializers.
    gazer::Type& handleArrayType(const llvm::ArrayType* type) override {
        llvm_unreachable("Pointers and arrays are not supported right now");
    }
};

class SimpleMemoryModel : public MemoryModel {
    SimpleMemoryInstructionHandler handler;

public:

    SimpleMemoryModel(
        GazerContext& context,
        const LLVMFrontendSettings& settings,
        llvm::Module& module) : handler(context, settings, module) {}

    /// Returns the memory instruction translator of this memory model.
    MemoryInstructionHandler& getMemoryInstructionHandler(llvm::Function& function) override
    {
        return handler;
    }

    /// Returns the type translator of this memory model.
    MemoryTypeTranslator& getMemoryTypeTranslator() override {
        return handler;
    }

    ~SimpleMemoryModel() override = default;
};

} // namespace

std::unique_ptr<MemoryModel> gazer::CreateSimpleMemoryModel(
    GazerContext& context,
    const LLVMFrontendSettings& settings,
    llvm::Module& module
) {
    return std::make_unique<SimpleMemoryModel>(context, settings, module);
}