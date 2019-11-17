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

// ValueOrMemoryObject
//==------------------------------------------------------------------------==//

/// A wrapper class which can either hold an LLVM IR value or a gazer memory object definition.
class ValueOrMemoryObject
{
    // Note: all of this probably could be done more easily by using DerivedUser in MemoryObject,
    // however this method seems less fragile, even if it introduces considerably more boilerplate.
public:
    /* implicit */ ValueOrMemoryObject(const llvm::Value* value)
        : mVariant(value)
    {}

    /* implicit */ ValueOrMemoryObject(const MemoryObjectDef* def)
        : mVariant(def)
    {}

    ValueOrMemoryObject(const ValueOrMemoryObject&) = default;
    ValueOrMemoryObject& operator=(const ValueOrMemoryObject&) = default;

    // We do not provide conversion operators, as an unchecked conversion may lead to runtime errors.
    operator const llvm::Value*() = delete;
    operator const MemoryObjectDef*() = delete;

    bool isValue() const { return std::holds_alternative<const llvm::Value*>(mVariant); }
    bool isMemoryObjectDef() const { return std::holds_alternative<const MemoryObjectDef*>(mVariant); }

    bool operator==(const ValueOrMemoryObject& rhs) const { return mVariant == rhs.mVariant; }
    bool operator!=(const ValueOrMemoryObject& rhs) const { return mVariant != rhs.mVariant; }

    const llvm::Value* asValue() const { return std::get<const llvm::Value*>(mVariant); }
    const MemoryObjectDef* asMemoryObjectDef() const { return std::get<const MemoryObjectDef*>(mVariant); }

    bool hasName() const;

    /// Returns the name of the contained value or memory object
    std::string getName() const;

private:
    std::variant<const llvm::Value*, const MemoryObjectDef*> mVariant;
};

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const ValueOrMemoryObject& rhs);

} // end namespace gazer

namespace llvm
{

// Some LLVM-specific magic to make DenseMap and isa<> work seamlessly with ValueOrMemoryObject.
// Note that while custom isa<> works, currently there is no support for cast<> or dynamic_cast<>.
template<>
struct DenseMapInfo<gazer::ValueOrMemoryObject>
{
    // Empty and tombstone will be represented by a variant holding an invalid Value pointer.
    static inline gazer::ValueOrMemoryObject getEmptyKey() {
        return DenseMapInfo<Value*>::getEmptyKey();
    }

    static inline gazer::ValueOrMemoryObject getTombstoneKey() {
        return DenseMapInfo<Value*>::getTombstoneKey();
    }

    static unsigned getHashValue(const gazer::ValueOrMemoryObject& val) {
        // This is a bit weird, but needed to get the pointer value from the variant properly.
        if (val.isValue()) {
            return DenseMapInfo<llvm::Value*>::getHashValue(val.asValue());
        }

        return DenseMapInfo<gazer::MemoryObjectDef*>::getHashValue(val.asMemoryObjectDef());
    }

    static bool isEqual(const gazer::ValueOrMemoryObject& lhs, const gazer::ValueOrMemoryObject& rhs) {
        return lhs == rhs;
    }
};

template<class To>
struct isa_impl<To, gazer::ValueOrMemoryObject, std::enable_if_t<std::is_base_of_v<Value, To>>>
{
    static inline bool doit(const gazer::ValueOrMemoryObject& val) {
        if (!val.isValue()) {
            return false;
        }

        return To::classof(val.asValue());
    }
};

template<class To>
struct isa_impl<To, gazer::ValueOrMemoryObject, std::enable_if_t<std::is_base_of_v<gazer::MemoryObjectDef, To>>>
{
    static inline bool doit(const gazer::ValueOrMemoryObject& val) {
        if (!val.isMemoryObjectDef()) {
            return false;
        }

        return To::classof(val.asMemoryObjectDef());
    }
};

} // end namespace llvm

namespace gazer
{

// Extension points
//==------------------------------------------------------------------------==//
// If a client (such as a memory model) wishes to hook into the CFA generation
// process, it may do so through extension points.
// Each hook will receive one of these extension point objects at the time
// they are called.

namespace llvm2cfa
{

class CfaGenInfo;

class ExtensionPoint
{
public:
    ExtensionPoint(CfaGenInfo& genInfo)
        : mInfo(genInfo)
    {}

    ExtensionPoint(const ExtensionPoint&) = delete;
    ExtensionPoint& operator=(const ExtensionPoint&) = delete;

    const Cfa& getCfa() const;

    llvm::Loop* getSourceLoop() const;
    llvm::Function* getSourceFunction() const;

protected:
    CfaGenInfo& mInfo;
};

/// This extension can be used to insert additional variables into the CFA at the
/// beginning of the generation process.
class VariableDeclExtensionPoint : public ExtensionPoint
{
public:
    using ExtensionPoint::ExtensionPoint;

    Variable* createInput(ValueOrMemoryObject val, Type& type, const std::string& suffix = "");
    Variable* createLocal(ValueOrMemoryObject val, Type& type, const std::string& suffix = "");

    /// \brief Creates an input variable which will be handled according to the
    /// transformation rules used for PHI nodes.
    Variable* createPhiInput(ValueOrMemoryObject val, Type& type, const std::string& suffix = "");

    /// Marks an already declared variable as output.
    void markOutput(ValueOrMemoryObject val, Variable* variable);
};

class GenerationStepExtensionPoint : public ExtensionPoint
{
public:
    GenerationStepExtensionPoint(
        CfaGenInfo& genInfo,
        std::vector<VariableAssignment>& assignmentList,
        std::function<ExprPtr(const llvm::Value*)> operandTranslator
    ) : ExtensionPoint(genInfo),
        mCurrentAssignmentList(assignmentList),
        mOperandTranslator(operandTranslator)
    {}

    Variable* getVariableFor(ValueOrMemoryObject val);
    void insertAssignment(VariableAssignment assignment);

    ExprPtr operand(ValueOrMemoryObject val);
private:
    std::vector<VariableAssignment>& mCurrentAssignmentList;
    std::function<ExprPtr(const llvm::Value*)> mOperandTranslator;
};

} // end namespace llvm2cfa



// Memory instruction handling
//==------------------------------------------------------------------------==//

/// An interface for translating memory instructions.
class MemoryInstructionTranslator
{
public:
    explicit MemoryInstructionTranslator(memory::MemorySSA& memSSA)
        : mMemorySSA(memSSA)
    {}

    // Variables

    /// Declares all input/output/local variables that should be inserted into \p cfa.
    virtual void declareProcedureVariables(llvm2cfa::VariableDeclExtensionPoint& extensionPoint) = 0;

    // Types
    virtual gazer::Type& handlePointerType(const llvm::PointerType* type) = 0;
    virtual gazer::Type& handleArrayType(const llvm::ArrayType* type) = 0;

    // Definitions
    virtual void translateStore(
        memory::StoreDef& def, Variable* variable, ExprPtr valueOperand, ExprPtr pointerOperand) = 0;

    // Uses
    virtual ExprPtr translateLoad(
        memory::LoadUse& use, Variable* variable, ExprPtr pointerOperand
    ) = 0;

private:
    memory::MemorySSA& mMemorySSA;
};

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

    ModuleToAutomataPass(GazerContext& context, LLVMFrontendSettings settings)
        : ModulePass(ID), mContext(context), mSettings(settings)
    {}

    void getAnalysisUsage(llvm::AnalysisUsage& au) const override;

    bool runOnModule(llvm::Module& module) override;

    llvm::StringRef getPassName() const override {
        return "Module to automata transformation";
    }

    AutomataSystem& getSystem() { return *mSystem; }
    llvm::DenseMap<llvm::Value*, Variable*>& getVariableMap() { return mVariables; }
    CfaToLLVMTrace& getTraceInfo() { return mTraceInfo; }

private:
    std::unique_ptr<AutomataSystem> mSystem;
    llvm::DenseMap<llvm::Value*, Variable*> mVariables;
    CfaToLLVMTrace mTraceInfo;
    GazerContext& mContext;
    LLVMFrontendSettings mSettings;
};

std::unique_ptr<AutomataSystem> translateModuleToAutomata(
    llvm::Module& module,
    LLVMFrontendSettings settings,
    llvm::DenseMap<llvm::Function*, llvm::LoopInfo*>& loopInfos,
    GazerContext& context,
    MemoryModel& memoryModel,
    llvm::DenseMap<llvm::Value*, Variable*>& variables,
    CfaToLLVMTrace& blockEntries
);

llvm::Pass* createCfaPrinterPass();

llvm::Pass* createCfaViewerPass();

}

#endif //GAZER_MODULETOAUTOMATA_H
