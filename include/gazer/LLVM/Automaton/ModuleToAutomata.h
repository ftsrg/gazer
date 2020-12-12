//==- ModuleToAutomata.h ----------------------------------------*- C++ -*--==//
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
#ifndef GAZER_MODULETOAUTOMATA_H
#define GAZER_MODULETOAUTOMATA_H

#include "gazer/Automaton/Cfa.h"
#include "gazer/LLVM/Memory/MemoryObject.h"
#include "gazer/LLVM/Memory/ValueOrMemoryObject.h"
#include "gazer/LLVM/LLVMFrontendSettings.h"
#include "gazer/LLVM/LLVMTraceBuilder.h"

#include <llvm/Pass.h>

#include <variant>

namespace llvm
{
    class Value;
    class Loop;
    class LoopInfo;
}

namespace gazer::llvm2cfa
{
    class GenerationContext;
} // end namespace gazer::llvm2cfa

namespace gazer
{

class MemoryModel;

// Extension points
//==------------------------------------------------------------------------==//
// If a client (such as a memory model) wishes to hook into the CFA generation
// process, it may do so through extension points. Each hook will receive one
// of these extension point objects at the time they are called.

namespace llvm2cfa
{

using LoopInfoFuncTy = std::function<llvm::LoopInfo*(const llvm::Function*)>;

class CfaGenInfo;

class ExtensionPoint
{
protected:
    ExtensionPoint(CfaGenInfo& genInfo)
        : mGenInfo(genInfo)
    {}

public:
    ExtensionPoint(const ExtensionPoint&) = default;
    ExtensionPoint& operator=(const ExtensionPoint&) = delete;

    const Cfa& getCfa() const;

    llvm::Loop* getSourceLoop() const;
    llvm::Function* getSourceFunction() const;

    llvm::Function* getParent() const;

    bool isEntryProcedure() const;

protected:
    CfaGenInfo& mGenInfo;
};

/// An extension point which may only query variables from the target automaton.
class AutomatonInterfaceExtensionPoint : public ExtensionPoint
{
public:
    explicit AutomatonInterfaceExtensionPoint(CfaGenInfo& genInfo)
        : ExtensionPoint(genInfo)
    {}

    Variable* getVariableFor(ValueOrMemoryObject val);
    Variable* getInputVariableFor(ValueOrMemoryObject val);
    Variable* getOutputVariableFor(ValueOrMemoryObject val);
};

/// This extension can be used to insert additional variables into the CFA at the
/// beginning of the generation process.
class VariableDeclExtensionPoint : public AutomatonInterfaceExtensionPoint
{
public:
    VariableDeclExtensionPoint(CfaGenInfo& genInfo)
        : AutomatonInterfaceExtensionPoint(genInfo)
    {}

    Variable* createInput(ValueOrMemoryObject val, Type& type, const std::string& suffix = "");
    Variable* createLocal(ValueOrMemoryObject val, Type& type, const std::string& suffix = "");

    /// \brief Creates an input variable which will be handled according to the
    /// transformation rules used for PHI nodes.
    Variable* createPhiInput(ValueOrMemoryObject val, Type& type, const std::string& suffix = "");

    /// Marks an already declared variable as output.
    void markOutput(ValueOrMemoryObject val, Variable* variable);
};

/// Variable declaration extension point for loops.
class LoopVarDeclExtensionPoint : public VariableDeclExtensionPoint
{
public:
    using VariableDeclExtensionPoint::VariableDeclExtensionPoint;

    /// Marks an already declared variable as a loop output, and creates an auxiliary
    /// variable to hold the output value.
    void createLoopOutput(
        ValueOrMemoryObject val, Variable* output, const llvm::Twine& suffix = "_out");
};

/// An extension point which can access the representations of already-translated instructions
/// and insert assignments in the current transformation step.
class GenerationStepExtensionPoint : public AutomatonInterfaceExtensionPoint
{
public:
    using AutomatonInterfaceExtensionPoint::AutomatonInterfaceExtensionPoint;

    /// Creates a new local variable into the current automaton.
    Variable* createAuxiliaryVariable(const std::string& name, Type& type);

    virtual ExprPtr getAsOperand(ValueOrMemoryObject val) = 0;

    /// Attempts to inline and eliminate a given variable from the CFA.
    virtual bool tryToEliminate(ValueOrMemoryObject val, Variable* variable, const ExprPtr& expr) = 0;

    virtual void insertAssignment(Variable* variable, const ExprPtr& value) = 0;

    virtual void splitCurrentTransition(const ExprPtr& guard) = 0;
};

} // end namespace llvm2cfa

// Traceability
//==------------------------------------------------------------------------==//

/// Provides a mapping between CFA locations/variables and LLVM values.
class CfaToLLVMTrace
{
    friend class llvm2cfa::GenerationContext;
public:
    enum LocationKind { Location_Unknown, Location_Entry, Location_Exit };

    struct BlockToLocationInfo
    {
        const llvm::BasicBlock* block;
        LocationKind kind;
    };

    struct ValueMappingInfo
    {
        llvm::DenseMap<ValueOrMemoryObject, ExprPtr> values;
    };

    BlockToLocationInfo getBlockFromLocation(Location* loc) {
        return mLocationsToBlocks.lookup(loc);
    }

    ExprPtr getExpressionForValue(const Cfa* parent, const llvm::Value* value);
    Variable* getVariableForValue(const Cfa* parent, const llvm::Value* value);

private:
    llvm::DenseMap<const Location*, BlockToLocationInfo> mLocationsToBlocks;
    llvm::DenseMap<const Cfa*, ValueMappingInfo> mValueMaps;
};

// LLVM pass
//==------------------------------------------------------------------------==//

/// ModuleToAutomataPass - translates an LLVM IR module into an automata system.
class ModuleToAutomataPass : public llvm::ModulePass
{
public:
    static char ID;

    ModuleToAutomataPass(GazerContext& context, LLVMFrontendSettings& settings)
        : ModulePass(ID), mContext(context), mSettings(settings)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override;

    bool runOnModule(llvm::Module& module) override;

    llvm::StringRef getPassName() const override {
        return "Module to automata transformation";
    }

    AutomataSystem& getSystem() { return *mSystem; }
    CfaToLLVMTrace& getTraceInfo() { return mTraceInfo; }

private:
    std::unique_ptr<AutomataSystem> mSystem;
    CfaToLLVMTrace mTraceInfo;
    GazerContext& mContext;
    LLVMFrontendSettings& mSettings;
};

class SpecialFunctions;

std::unique_ptr<AutomataSystem> translateModuleToAutomata(
    llvm::Module& module,
    const LLVMFrontendSettings& settings,
    llvm2cfa::LoopInfoFuncTy loopInfos,
    GazerContext& context,
    MemoryModel& memoryModel,
    CfaToLLVMTrace& blockEntries,
    const SpecialFunctions* specialFunctions = nullptr
);

llvm::Pass* createCfaPrinterPass();

llvm::Pass* createCfaViewerPass();

}

#endif //GAZER_MODULETOAUTOMATA_H
