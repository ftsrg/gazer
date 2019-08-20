#ifndef GAZER_SRC_FUNCTIONTOAUTOMATA_H
#define GAZER_SRC_FUNCTIONTOAUTOMATA_H

#include "gazer/Core/GazerContext.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Automaton/Cfa.h"
#include "gazer/LLVM/Analysis/MemoryObject.h"

#include "gazer/LLVM/InstToExpr.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/ADT/MapVector.h>

namespace gazer
{

extern llvm::cl::opt<bool> Trace;

using ValueToVariableMap = llvm::DenseMap<llvm::Value*, Variable*>;

/// Stores information about loops which were transformed to automata.
struct CfaGenInfo
{
    llvm::MapVector<const llvm::Value*, Variable*> Inputs;
    llvm::MapVector<const llvm::Value*, Variable*> Outputs;
    llvm::MapVector<const llvm::Value*, Variable*> PhiInputs;
    llvm::MapVector<const llvm::Value*, VariableAssignment> LoopOutputs;

    llvm::DenseMap<llvm::Value*, Variable*> Locals;
    llvm::DenseMap<llvm::BasicBlock*, std::pair<Location*, Location*>> Blocks;
    llvm::DenseMap<Location*, llvm::BasicBlock*> ReverseBlockMap;

    Cfa* Automaton;

    // For automata with multiple exit paths, this variable tells us which was taken.
    Variable* ExitVariable = nullptr;
    llvm::SmallDenseMap<llvm::BasicBlock*, ExprRef<BvLiteralExpr>, 4> ExitBlocks;

    CfaGenInfo() = default;
    CfaGenInfo(CfaGenInfo&&) = default;

    CfaGenInfo(const CfaGenInfo&) = delete;
    CfaGenInfo& operator=(const CfaGenInfo&) = delete;

    void addInput(llvm::Value* value, Variable* variable) {
        Inputs[value] = variable;
    }
    void addPhiInput(llvm::Value* value, Variable* variable) {
        PhiInputs[value] = variable;
    }
    void addLocal(llvm::Value* value, Variable* variable) {
        Locals[value] = variable;
    }

    void addReverseBlockIfTraceEnabled(llvm::BasicBlock* bb, Location* loc) {
        if (Trace) {
            this->ReverseBlockMap[loc] = bb;
        }
    }

    Variable* findVariable(const llvm::Value* value) {
        Variable* result = Inputs.lookup(value);
        if (result != nullptr) { return result; }

        result = PhiInputs.lookup(value);
        if (result != nullptr) { return result; }

        result = Locals.lookup(value);
        if (result != nullptr) { return result; }

        return nullptr;
    }

    Variable* findInput(const llvm::Value* value) {
        Variable* result = Inputs.lookup(value);
        if (result != nullptr) { return result; }

        return PhiInputs.lookup(value);
    }

    Variable* findOutput(const llvm::Value* value) {
        return Outputs.lookup(value);
    }

    Variable* findLocal(const llvm::Value* value) {
        return Locals.lookup(value);   
    }

    bool hasInput(llvm::Value* value) {
        return Inputs.find(value) != Inputs.end();
    }
    bool hasLocal(const llvm::Value* value) {
        return Locals.count(value) != 0;
    }
};

/// Helper structure for CFA generation information.
struct GenerationContext
{
    llvm::DenseMap<llvm::Function*, CfaGenInfo> FunctionMap;
    llvm::DenseMap<llvm::Loop*, CfaGenInfo> LoopMap;
    llvm::LoopInfo* LoopInfo;
    ModuleToAutomataSettings Settings;

    AutomataSystem& System;
    MemoryModel& TheMemoryModel;

public:
    explicit GenerationContext(AutomataSystem& system, MemoryModel& memoryModel)
        : System(system), TheMemoryModel(memoryModel)
    {}

    GenerationContext(const GenerationContext&) = delete;
    GenerationContext& operator=(const GenerationContext&) = delete;
};

class ModuleToCfa final
{
public:

    using LoopInfoMapTy = std::unordered_map<llvm::Function*, llvm::LoopInfo*>;

    static constexpr char FunctionReturnValueName[] = "RET_VAL";
    static constexpr char LoopOutputSelectorName[] = "__output_selector";

    ModuleToCfa(
        llvm::Module& module,
        LoopInfoMapTy& loops,
        GazerContext& context,
        MemoryModel& memoryModel,
        ModuleToAutomataSettings settings
    )
        : mModule(module),
        mLoops(loops), mContext(context),
        mMemoryModel(memoryModel), mSettings(settings)
    {
        mSystem = std::make_unique<AutomataSystem>(context);
    }

    std::unique_ptr<AutomataSystem> generate(
        llvm::DenseMap<llvm::Value*, Variable*>& variables,
        llvm::DenseMap<Location*, llvm::BasicBlock*>& blockEntries
    );

private:
    llvm::Module& mModule;
    LoopInfoMapTy& mLoops;

    GazerContext& mContext;
    std::unique_ptr<AutomataSystem> mSystem;

    MemoryModel& mMemoryModel;
    ModuleToAutomataSettings mSettings;

    // Generation helpers
    std::unordered_map<llvm::Function*, Cfa*> mFunctionMap;
    std::unordered_map<llvm::Loop*, Cfa*> mLoopMap;
};

class BlocksToCfa : public InstToExpr
{
public:
    BlocksToCfa(
        GenerationContext& generationContext,
        CfaGenInfo& genInfo,
        llvm::ArrayRef<llvm::BasicBlock*> blocks,
        Cfa* cfa,
        ExprBuilder& exprBuilder
    ) : InstToExpr(exprBuilder, generationContext.TheMemoryModel),
        mGenCtx(generationContext),
        mGenInfo(genInfo),
        mBlocks(blocks),
        mCfa(cfa)
    {}

    void encode(llvm::BasicBlock* entry);

protected:
    Variable* getVariable(const llvm::Value* value) override;
    ExprPtr lookupInlinedVariable(const llvm::Value* value) override;

private:
    GazerContext& getContext() const { return mGenCtx.System.getContext(); }
private:

    bool tryToEliminate(llvm::Instruction& inst, ExprPtr expr);

    void insertOutputAssignments(CfaGenInfo& callee, std::vector<VariableAssignment>& outputArgs);
    void insertPhiAssignments(llvm::BasicBlock* source, llvm::BasicBlock* target, std::vector<VariableAssignment>& phiAssignments);

    void handleSuccessor(
        llvm::BasicBlock* succ, ExprPtr& succCondition, llvm::BasicBlock* parent,
        llvm::BasicBlock* entryBlock, Location* exit
    );
    void createExitTransition(llvm::BasicBlock* target, Location* pred, ExprPtr succCondition);
    ExprPtr getExitCondition(llvm::BasicBlock* target, Variable* exitSelector, CfaGenInfo& nestedInfo);
private:
    GenerationContext& mGenCtx;
    CfaGenInfo& mGenInfo;
    llvm::ArrayRef<llvm::BasicBlock*> mBlocks;
    Cfa* mCfa;
    unsigned mCounter = 0;
    llvm::DenseMap<llvm::Value*, ExprPtr> mInlinedVars;
    llvm::DenseSet<Variable*> mEliminatedVarsSet;
};

class TraceFromCfaToLLVM
{
public:
    llvm::BasicBlock* getBasicBlock(Location* loc);
    llvm::Value* getValueFromVariable(Variable* variable);

private:
    Cfa* mCfa;

};

} // end namespace gazer

#endif //GAZER_SRC_FUNCTIONTOAUTOMATA_H
