#ifndef GAZER_SRC_FUNCTIONTOAUTOMATA_H
#define GAZER_SRC_FUNCTIONTOAUTOMATA_H

#include "gazer/Core/GazerContext.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Automaton/Cfa.h"
#include "gazer/LLVM/Analysis/MemoryObject.h"

#include "gazer/LLVM/Automaton/InstToExpr.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/ADT/MapVector.h>

#include <variant>

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

    llvm::MapVector<const llvm::BasicBlock*, std::pair<Location*, Location*>> Blocks;

    llvm::DenseMap<llvm::Value*, Variable*> Locals;
    llvm::DenseMap<Location*, llvm::BasicBlock*> ReverseBlockMap;

    Cfa* Automaton;
    std::variant<llvm::Function*, llvm::Loop*> Source;

    // For automata with multiple exit paths, this variable tells us which was taken.
    Variable* ExitVariable = nullptr;
    llvm::SmallDenseMap<const llvm::BasicBlock*, ExprRef<LiteralExpr>, 4> ExitBlocks;

public:
    CfaGenInfo() = default;
    CfaGenInfo(CfaGenInfo&&) = default;

    CfaGenInfo(const CfaGenInfo&) = delete;
    CfaGenInfo& operator=(const CfaGenInfo&) = delete;

    //--------------------- Sources ---------------------//
    llvm::Loop* getSourceLoop() const
    {
        if (std::holds_alternative<llvm::Loop*>(Source)) {
            return std::get<llvm::Loop*>(Source);
        }

        return nullptr;
    }
    
    llvm::Function* getSourceFunction() const
    {
        if (std::holds_alternative<llvm::Function*>(Source)) {
            return std::get<llvm::Function*>(Source);
        }

        return nullptr;
    }    

    //--------------------- Variables ---------------------//
    void addInput(llvm::Value* value, Variable* variable) { Inputs[value] = variable; }
    void addPhiInput(llvm::Value* value, Variable* variable) { PhiInputs[value] = variable; }
    void addLocal(llvm::Value* value, Variable* variable) { Locals[value] = variable; }

    Variable* findVariable(const llvm::Value* value) {
        Variable* result = Inputs.lookup(value);
        if (result != nullptr) { return result; }

        result = PhiInputs.lookup(value);
        if (result != nullptr) { return result; }

        result = Locals.lookup(value);
        if (result != nullptr) { return result; }

        return nullptr;
    }

    Variable* findInput(const llvm::Value* value)
    {
        Variable* result = Inputs.lookup(value);
        if (result != nullptr) { return result; }

        return PhiInputs.lookup(value);
    }

    Variable* findOutput(const llvm::Value* value) { return Outputs.lookup(value);}
    Variable* findLocal(const llvm::Value* value) { return Locals.lookup(value); }

    bool hasInput(llvm::Value* value) { return Inputs.count(value) != 0; }
    bool hasLocal(const llvm::Value* value) { return Locals.count(value) != 0; }

    //---------------------- Traces -----------------------//
    void addReverseBlockIfTraceEnabled(llvm::BasicBlock* bb, Location* loc) {
        if (Trace) {
            this->ReverseBlockMap[loc] = bb;
        }
    }
};

/// Helper structure for CFA generation information.
class GenerationContext
{
public:
    using LoopInfoMapTy = llvm::DenseMap<llvm::Function*, llvm::LoopInfo*>;
    using VariantT = std::variant<llvm::Function*, llvm::Loop*>;

public:
    GenerationContext(
        AutomataSystem& system,
        MemoryModel& memoryModel,
        LoopInfoMapTy loopInfos,
        LLVMFrontendSettings settings
    )
        : mSystem(system), mMemoryModel(memoryModel),
        mLoopInfos(loopInfos), mSettings(settings)
    {}

    GenerationContext(const GenerationContext&) = delete;
    GenerationContext& operator=(const GenerationContext&) = delete;

    CfaGenInfo& createLoopCfaInfo(Cfa* cfa, llvm::Loop* loop)
    {
        CfaGenInfo& info = mProcedures.try_emplace(loop).first->second;
        info.Automaton = cfa;
        info.Source = loop;

        return info;
    }

    CfaGenInfo& createFunctionCfaInfo(Cfa* cfa, llvm::Function* function)
    {
        CfaGenInfo& info = mProcedures.try_emplace(function).first->second;
        info.Automaton = cfa;
        info.Source = function;        

        return info;
    }

    void addVariable(llvm::Value* value, Variable* variable) {
        mVariables[value] = variable;
    }

    CfaGenInfo& getLoopCfa(llvm::Loop* loop) { return getInfoFor(loop); }
    CfaGenInfo& getFunctionCfa(llvm::Function* function) { return getInfoFor(function); }

    llvm::iterator_range<typename std::unordered_map<VariantT, CfaGenInfo>::iterator> procedures()
    {
        return llvm::make_range(mProcedures.begin(), mProcedures.end());
    }

    llvm::LoopInfo* getLoopInfoFor(const llvm::Function* function)
    {
        return mLoopInfos.lookup(function);
    }
    
    AutomataSystem& getSystem() const { return mSystem; }
    MemoryModel& getMemoryModel() const { return mMemoryModel; }
    LLVMFrontendSettings& getSettings() { return mSettings; }

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
    LoopInfoMapTy mLoopInfos;
    LLVMFrontendSettings mSettings;
    std::unordered_map<VariantT, CfaGenInfo> mProcedures;
    llvm::DenseMap<llvm::Value*, Variable*> mVariables;
};

class ModuleToCfa final
{
public:
    static constexpr char FunctionReturnValueName[] = "RET_VAL";
    static constexpr char LoopOutputSelectorName[] = "__output_selector";

    ModuleToCfa(
        llvm::Module& module,
        GenerationContext::LoopInfoMapTy& loops,
        GazerContext& context,
        MemoryModel& memoryModel,
        LLVMFrontendSettings settings
    );

    std::unique_ptr<AutomataSystem> generate(
        llvm::DenseMap<llvm::Value*, Variable*>& variables,
        llvm::DenseMap<Location*, llvm::BasicBlock*>& blockEntries
    );

protected:
    void createAutomata();

private:
    llvm::Module& mModule;

    GazerContext& mContext;
    MemoryModel& mMemoryModel;
    LLVMFrontendSettings mSettings;

    std::unique_ptr<AutomataSystem> mSystem;
    GenerationContext mGenCtx;
    std::unique_ptr<ExprBuilder> mExprBuilder;

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
        ExprBuilder& exprBuilder
    ) : InstToExpr(exprBuilder, generationContext.getMemoryModel(), generationContext.getSettings()),
        mGenCtx(generationContext),
        mGenInfo(genInfo),
        mCfa(genInfo.Automaton)
    {
        if (auto function = genInfo.getSourceFunction()) {
            mEntryBlock = &function->getEntryBlock();
        } else if (auto loop = genInfo.getSourceLoop()) {
            mEntryBlock = loop->getHeader();
        } else {
            llvm_unreachable("A CFA source can only be a function or a loop!");
        }

        assert(mGenInfo.Blocks.count(mEntryBlock) != 0 && "Entry block must be in the block map!");
    }

    void encode();

protected:
    Variable* getVariable(const llvm::Value* value) override;
    ExprPtr lookupInlinedVariable(const llvm::Value* value) override;

private:
    GazerContext& getContext() const { return mGenCtx.getSystem().getContext(); }

private:
    bool tryToEliminate(const llvm::Instruction& inst, ExprPtr expr);

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
    void createExitTransition(const llvm::BasicBlock* target, Location* pred, const ExprPtr& succCondition);
    ExprPtr getExitCondition(const llvm::BasicBlock* target, Variable* exitSelector, CfaGenInfo& nestedInfo);

    size_t getNumUsesInBlocks(const llvm::Instruction* inst) const;

private:
    GenerationContext& mGenCtx;
    CfaGenInfo& mGenInfo;
    Cfa* mCfa;
    unsigned mCounter = 0;
    llvm::DenseMap<const llvm::Value*, ExprPtr> mInlinedVars;
    llvm::DenseSet<Variable*> mEliminatedVarsSet;
    llvm::BasicBlock* mEntryBlock;
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
