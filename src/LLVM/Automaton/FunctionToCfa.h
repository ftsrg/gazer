#ifndef GAZER_SRC_FUNCTIONTOAUTOMATA_H
#define GAZER_SRC_FUNCTIONTOAUTOMATA_H

#include <gazer/Core/GazerContext.h>
#include <gazer/Core/Expr/ExprBuilder.h>
#include <gazer/Automaton/Cfa.h>

#include <llvm/IR/Function.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Analysis/LoopInfo.h>

namespace gazer
{

using ValueToVariableMap = llvm::DenseMap<llvm::Value*, Variable*>;

Type& typeFromLLVMType(const llvm::Type* type, GazerContext& context);

class FunctionToCfa
{
public:
    FunctionToCfa(
        llvm::Function& function,
        GazerContext& context,
        ExprBuilder* builder,
        ValueToVariableMap& variables,
        llvm::LoopInfo& loopInfo
    );

public:
    void encode(Cfa* cfa);

private:
    llvm::Function& mFunction;
    GazerContext& mContext;
    ExprBuilder* mExprBuilder;
    ValueToVariableMap& mVariables;
    llvm::LoopInfo& mLoopInfo;

    AutomataSystem* mSystem;
    std::unordered_map<llvm::BasicBlock*, std::pair<Location*, Location*>> mBlockMap;
};

/// Stores information about loops which were transformed to automata.
struct CfaGenInfo
{
    llvm::SmallDenseMap<llvm::Value*, Variable*, 8> Inputs;
    llvm::SmallDenseMap<llvm::Value*, Variable*, 8> Outputs;
    llvm::SmallDenseMap<llvm::Value*, Variable*, 4> PhiInputs;
    llvm::DenseMap<llvm::Value*, Variable*> Locals;
    llvm::DenseMap<llvm::BasicBlock*, std::pair<Location*, Location*>> Blocks;

    Cfa* Automaton;

    // For automata with multiple exit paths, this variable tells us which was taken.
    Variable* ExitVariable = nullptr;

    CfaGenInfo() = default;
    CfaGenInfo(CfaGenInfo&&) = default;

    CfaGenInfo(const CfaGenInfo&) = delete;
    CfaGenInfo& operator=(const CfaGenInfo&) = delete;
};

/// Helper structure for CFA generation information.
struct GenerationContext
{
    llvm::DenseMap<llvm::Function*, CfaGenInfo> FunctionMap;
    llvm::DenseMap<llvm::Loop*, CfaGenInfo> LoopMap;
    llvm::LoopInfo* LoopInfo;

    AutomataSystem& System;

public:
    explicit GenerationContext(AutomataSystem& system);

    GenerationContext(const GenerationContext&) = delete;
    GenerationContext& operator=(const GenerationContext&) = delete;
};

class BlocksToCfa
{
public:
    BlocksToCfa(
        GenerationContext& generationContext,
        CfaGenInfo& genInfo,
        llvm::ArrayRef<llvm::BasicBlock*> blocks,
        Cfa* cfa,
        ExprBuilder& exprBuilder
    ) : mGenCtx(generationContext),
        mGenInfo(genInfo),
        mBlocks(blocks),
        mCfa(cfa),
        mExprBuilder(exprBuilder)
    {}

    void encode(llvm::BasicBlock* entry);

    ExprPtr transform(llvm::Instruction& inst);

    ExprPtr visitBinaryOperator(llvm::BinaryOperator& binop);
    ExprPtr visitSelectInst(llvm::SelectInst& select);
    ExprPtr visitICmpInst(llvm::ICmpInst& icmp);
    ExprPtr visitFCmpInst(llvm::FCmpInst& fcmp);
    ExprPtr visitCastInst(llvm::CastInst& cast);
    ExprPtr visitCallInst(llvm::CallInst& call);

private:
    ExprPtr integerCast(llvm::CastInst& cast, ExprPtr operand, unsigned int width);
    ExprPtr operand(const llvm::Value* value);

    Variable* getVariable(const llvm::Value* value);
    ExprPtr asBool(ExprPtr operand);

    ExprPtr asInt(ExprPtr operand, unsigned int bits);
    ExprPtr castResult(ExprPtr expr, const Type& type);

    void insertPhiAssignments(llvm::BasicBlock* source, llvm::BasicBlock* target, std::vector<VariableAssignment>& phiAssignments);

private:
    GenerationContext& mGenCtx;
    CfaGenInfo& mGenInfo;
    llvm::ArrayRef<llvm::BasicBlock*> mBlocks;
    Cfa* mCfa;
    ExprBuilder& mExprBuilder;
};

}

#endif //GAZER_SRC_FUNCTIONTOAUTOMATA_H
