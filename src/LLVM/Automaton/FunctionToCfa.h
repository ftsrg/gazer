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
#ifndef GAZER_SRC_FUNCTIONTOAUTOMATA_H
#define GAZER_SRC_FUNCTIONTOAUTOMATA_H

#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/Automaton/SpecialFunctions.h"
#include "gazer/Core/GazerContext.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Automaton/Cfa.h"

#include "gazer/LLVM/Memory/MemoryObject.h"
#include "gazer/LLVM/Memory/MemoryModel.h"
#include "gazer/LLVM/Automaton/InstToExpr.h"
#include "gazer/LLVM/LLVMTraceBuilder.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/ADT/MapVector.h>

#include <variant>

namespace gazer
{

extern llvm::cl::opt<bool> PrintTrace;

namespace llvm2cfa
{

using ValueToVariableMap = llvm::DenseMap<llvm::Value*, Variable*>;

/// Stores information about loops which were transformed to automata.
class CfaGenInfo
{
public:
    GenerationContext& Context;
    llvm::MapVector<ValueOrMemoryObject, Variable*> Inputs;
    llvm::MapVector<ValueOrMemoryObject, Variable*> Outputs;
    llvm::MapVector<ValueOrMemoryObject, Variable*> PhiInputs;
    llvm::MapVector<ValueOrMemoryObject, VariableAssignment> LoopOutputs;

    llvm::MapVector<const llvm::BasicBlock*, std::pair<Location*, Location*>> Blocks;

    llvm::DenseMap<ValueOrMemoryObject, Variable*> Locals;

    Cfa* Automaton;
    std::variant<llvm::Function*, llvm::Loop*> Source;

    using ExitEdge = std::pair<const llvm::BasicBlock*, const llvm::BasicBlock*>;


    // For automata with multiple exit paths, this variable tells us which was taken.
    Variable* ExitVariable = nullptr;
    llvm::SmallDenseMap<ExitEdge, ExprRef<LiteralExpr>> ExitEdges;

    // For functions with return values
    Variable* ReturnVariable = nullptr;

public:
    CfaGenInfo(GenerationContext& context, Cfa* cfa, std::variant<llvm::Function*, llvm::Loop*> source)
        : Context(context), Automaton(cfa), Source(source)
    {}

    CfaGenInfo(CfaGenInfo&&) = default;

    CfaGenInfo(const CfaGenInfo&) = delete;
    CfaGenInfo& operator=(const CfaGenInfo&) = delete;

    // Blocks
    //==--------------------------------------------------------------------==//
    void addBlockToLocationsMapping(const llvm::BasicBlock* bb, Location* entry, Location* exit);

    // Sources
    //==--------------------------------------------------------------------==//
    bool isSourceLoop() const { return std::holds_alternative<llvm::Loop*>(Source); }
    bool isSourceFunction() const { return std::holds_alternative<llvm::Function*>(Source); }

    llvm::Loop* getSourceLoop() const
    {
        if (this->isSourceLoop()) {
            return std::get<llvm::Loop*>(Source);
        }

        return nullptr;
    }

    llvm::Function* getSourceFunction() const
    {
        if (this->isSourceFunction()) {
            return std::get<llvm::Function*>(Source);
        }

        return nullptr;
    }

    llvm::BasicBlock* getEntryBlock() const
    {
        if (auto function = getSourceFunction()) {
            return &function->getEntryBlock();
        }
        
        if (auto loop = getSourceLoop()) {
            return loop->getHeader();
        }

        llvm_unreachable("A CFA source can only be a function or a loop!");
    }

    // Variables
    //==--------------------------------------------------------------------==//
    void addInput(ValueOrMemoryObject value, Variable* variable)
    {
        Inputs[value] = variable;
        addVariableToContext(value, variable);
    }

    void addPhiInput(ValueOrMemoryObject value, Variable* variable)
    {
        PhiInputs[value] = variable;
        addVariableToContext(value, variable);
    }

    void addLocal(ValueOrMemoryObject value, Variable* variable)
    {
        Locals[value] = variable;
        addVariableToContext(value, variable);
    }

    Variable* findVariable(ValueOrMemoryObject value)
    {
        Variable* result = Inputs.lookup(value);
        if (result != nullptr) { return result; }

        result = PhiInputs.lookup(value);
        if (result != nullptr) { return result; }

        result = Locals.lookup(value);
        if (result != nullptr) { return result; }

        return nullptr;
    }

    Variable* findInput(ValueOrMemoryObject value)
    {
        Variable* result = Inputs.lookup(value);
        if (result != nullptr) { return result; }

        return PhiInputs.lookup(value);
    }

    Variable* findOutput(ValueOrMemoryObject value) { return Outputs.lookup(value);}
    Variable* findLocal(ValueOrMemoryObject value) { return Locals.lookup(value); }

    bool hasInput(llvm::Value* value) { return Inputs.count(value) != 0; }
    bool hasLocal(const llvm::Value* value) { return Locals.count(value) != 0; }

    MemoryInstructionHandler& getMemoryInstructionHandler() const;

private:
    void addVariableToContext(ValueOrMemoryObject value, Variable* variable);
};

class GenerationContext;

/// Helper structure for CFA generation information.
class GenerationContext
{
public:
    using VariantT = std::variant<llvm::Function*, llvm::Loop*>;

public:
    GenerationContext(
        AutomataSystem& system,
        MemoryModel& memoryModel,
        LLVMTypeTranslator& types,
        LoopInfoFuncTy loopInfos,
        const SpecialFunctions& specialFunctions,
        const LLVMFrontendSettings& settings
    ) : mSystem(system), mMemoryModel(memoryModel), mTypes(types),
        mLoopInfos(loopInfos), mSpecialFunctions(specialFunctions), mSettings(settings)
    {}

    GenerationContext(const GenerationContext&) = delete;
    GenerationContext& operator=(const GenerationContext&) = delete;

    CfaGenInfo& createLoopCfaInfo(Cfa* cfa, llvm::Loop* loop)
    {
        CfaGenInfo& info = mProcedures.try_emplace(loop, *this, cfa, loop).first->second;
        return info;
    }

    CfaGenInfo& createFunctionCfaInfo(Cfa* cfa, llvm::Function* function)
    {
        CfaGenInfo& info = mProcedures.try_emplace(function, *this, cfa, function).first->second;
        return info;
    }

    void addReverseBlockIfTraceEnabled(
        const llvm::BasicBlock* bb, Location* loc, CfaToLLVMTrace::LocationKind kind
    ) {
        if (mSettings.trace) {
            mTraceInfo.mLocationsToBlocks[loc] = { bb, kind };
        }
    }

    void addExprValueIfTraceEnabled(Cfa* cfa, ValueOrMemoryObject value, ExprPtr expr)
    {
        if (mSettings.trace) {
            mTraceInfo.mValueMaps[cfa].values[value] = std::move(expr);
        }
    }

    CfaGenInfo& getLoopCfa(llvm::Loop* loop) { return getInfoFor(loop); }
    CfaGenInfo& getFunctionCfa(llvm::Function* function) { return getInfoFor(function); }

    llvm::iterator_range<typename std::unordered_map<VariantT, CfaGenInfo>::iterator> procedures()
    {
        return llvm::make_range(mProcedures.begin(), mProcedures.end());
    }

    llvm::LoopInfo* getLoopInfoFor(const llvm::Function* function)
    {
        return mLoopInfos(function);
    }

    std::string uniqueName(const llvm::Twine& base = "");

    AutomataSystem& getSystem() const { return mSystem; }
    MemoryModel& getMemoryModel() const { return mMemoryModel; }
    LLVMTypeTranslator& getTypes() const { return mTypes; }
    const LLVMFrontendSettings& getSettings() { return mSettings; }
    CfaToLLVMTrace& getTraceInfo() { return mTraceInfo; }
    const SpecialFunctions& getSpecialFunctions() const { return mSpecialFunctions; }

private:
    CfaGenInfo& getInfoFor(VariantT key)
    {
        auto it = mProcedures.find(key);
        assert(it != mProcedures.end());

        return it->second;
    }

private:
    AutomataSystem& mSystem;
    MemoryModel& mMemoryModel;
    LLVMTypeTranslator& mTypes;
    LoopInfoFuncTy mLoopInfos;
    const SpecialFunctions& mSpecialFunctions;
    const LLVMFrontendSettings& mSettings;
    std::unordered_map<VariantT, CfaGenInfo> mProcedures;
    CfaToLLVMTrace mTraceInfo;
    unsigned mTmp = 0;
};

class ModuleToCfa final
{
public:
    static constexpr char FunctionReturnValueName[] = "RET_VAL";
    static constexpr char LoopOutputSelectorName[] = "__output_selector";

    ModuleToCfa(
        llvm::Module& module,
        LoopInfoFuncTy loops,
        GazerContext& context,
        MemoryModel& memoryModel,
        LLVMTypeTranslator& types,
        const SpecialFunctions& specialFunctions,
        const LLVMFrontendSettings& settings
    );

    std::unique_ptr<AutomataSystem> generate(CfaToLLVMTrace& cfaToLlvmTrace);

protected:
    void createAutomata();

    void declareLoopVariables(
        llvm::Loop* loop, CfaGenInfo& loopGenInfo,
        MemoryInstructionHandler& memoryInstHandler,
        llvm::ArrayRef<llvm::BasicBlock*> loopBlocks,
        llvm::ArrayRef<llvm::BasicBlock*> loopOnlyBlocks,
        llvm::DenseSet<llvm::BasicBlock*>& visitedBlocks
    );

private:
    llvm::Module& mModule;

    GazerContext& mContext;
    MemoryModel& mMemoryModel;
    const LLVMFrontendSettings& mSettings;

    std::unique_ptr<AutomataSystem> mSystem;
    GenerationContext mGenCtx;
    std::unique_ptr<ExprBuilder> mExprBuilder;

    // Generation helpers
    std::unordered_map<llvm::Function*, Cfa*> mFunctionMap;
    std::unordered_map<llvm::Loop*, Cfa*> mLoopMap;
};

class BlocksToCfa : public InstToExpr
{
protected:
    class ExtensionPointImpl : public GenerationStepExtensionPoint
    {
    public:
        ExtensionPointImpl(
            BlocksToCfa& blocksToCfa,
            std::vector<VariableAssignment>& assigns,
            Location** entry,
            Location** exit
        ) : GenerationStepExtensionPoint(blocksToCfa.mGenInfo),
            mBlocksToCfa(blocksToCfa), mAssigns(assigns), mEntry(entry), mExit(exit)
        {}

        ExprPtr getAsOperand(ValueOrMemoryObject val, Type* type) override;
        bool tryToEliminate(ValueOrMemoryObject val, Variable* variable, const ExprPtr& expr) override;
        void insertAssignment(Variable* variable, const ExprPtr& value) override;

        void splitCurrentTransition(const ExprPtr& guard) override;

        using guard_iterator = std::vector<ExprPtr>::const_iterator;
        llvm::iterator_range<guard_iterator> guards() const {
            return llvm::make_range(mGuards.begin(), mGuards.end());
        }

    private:
        BlocksToCfa& mBlocksToCfa;
        std::vector<VariableAssignment>& mAssigns;
        std::vector<ExprPtr> mGuards;
        Location** mEntry;
        Location** mExit;
    };
public:
    BlocksToCfa(
        GenerationContext& generationContext,
        CfaGenInfo& genInfo,
        ExprBuilder& exprBuilder
    );

    void encode();

protected:
    Variable* getVariable(ValueOrMemoryObject value) override;
    ExprPtr lookupInlinedVariable(ValueOrMemoryObject value) override;

private:
    GazerContext& getContext() const { return mGenCtx.getSystem().getContext(); }

private:
    bool tryToEliminate(ValueOrMemoryObject val, Variable* variable, const ExprPtr& expr);

    void insertOutputAssignments(CfaGenInfo& callee, std::vector<VariableAssignment>& outputArgs);
    void insertPhiAssignments(const llvm::BasicBlock* source, const llvm::BasicBlock* target, std::vector<VariableAssignment>& phiAssignments);

    bool handleCall(const llvm::CallInst* call, Location** entry, Location* exit, std::vector<VariableAssignment>& previousAssignments);
    void handleTerminator(const llvm::BasicBlock* bb, Location* entry, Location* exit);

    void handleSuccessor(
        const llvm::BasicBlock* succ,
        const ExprPtr& succCondition,
        const llvm::BasicBlock* parent,
        Location* exit
    );

    void createExitTransition(const llvm::BasicBlock* source, const llvm::BasicBlock* target, Location* pred, const ExprPtr& succCondition);

    void createCallToLoop(llvm::Loop* loop, const llvm::BasicBlock* source, const ExprPtr& condition, Location* exit);

    ExprPtr getExitCondition(const llvm::BasicBlock* source, const llvm::BasicBlock* target, Variable* exitSelector, CfaGenInfo& nestedInfo);
    size_t getNumUsesInBlocks(const llvm::Instruction* inst) const;

    ExtensionPointImpl createExtensionPoint(
        std::vector<VariableAssignment>& assignments, Location** entry, Location** exit);

private:
    GenerationContext& mGenCtx;
    CfaGenInfo& mGenInfo;
    Cfa* mCfa;
    unsigned mCounter = 0;
    llvm::DenseMap<ValueOrMemoryObject, ExprPtr> mInlinedVars;
    llvm::DenseSet<Variable*> mEliminatedVarsSet;
    llvm::BasicBlock* mEntryBlock;
};

} // end namespace llvm2cfa

} // end namespace gazer

#endif //GAZER_SRC_FUNCTIONTOAUTOMATA_H
