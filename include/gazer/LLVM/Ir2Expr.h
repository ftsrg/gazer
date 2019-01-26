#ifndef _GAZER_LLVM_IR2EXPR_H
#define _GAZER_LLVM_IR2EXPR_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/SymbolTable.h"
#include "gazer/Core/Expr/ExprBuilder.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/InstVisitor.h>

namespace gazer
{

class InstToExpr
{
public:
    using ValueToVariableMapT = llvm::DenseMap<const llvm::Value*, Variable*>;
public:
    InstToExpr(
        llvm::Function& function,
        GazerContext& context,
        ExprBuilder* builder,
        ValueToVariableMapT& variables,
        llvm::DenseMap<llvm::Value*, ExprPtr>& eliminatedValues
    );

    ExprPtr transform(llvm::Instruction& inst, size_t succIdx, llvm::BasicBlock* pred = nullptr);
    ExprPtr transform(llvm::Instruction& inst);

    ExprBuilder* getBuilder() const { return mExprBuilder; }

    const ValueToVariableMapT& getVariableMap() const { return mVariables; }

public:
    ExprPtr visitBinaryOperator(llvm::BinaryOperator &binop);
    ExprPtr visitSelectInst(llvm::SelectInst& select);
    ExprPtr visitICmpInst(llvm::ICmpInst& icmp);
    ExprPtr visitFCmpInst(llvm::FCmpInst& fcmp);
    ExprPtr visitCastInst(llvm::CastInst& cast);
    ExprPtr visitCallInst(llvm::CallInst& call);

    ExprPtr visitAllocaInst(llvm::AllocaInst& alloc);
    ExprPtr visitStoreInst(llvm::StoreInst& store);
    ExprPtr visitLoadInst(llvm::LoadInst& load);
    ExprPtr visitGetElementPtrInst(llvm::GetElementPtrInst& gep);
    
    ExprPtr handlePHINode(llvm::PHINode& phi, llvm::BasicBlock* pred);
    ExprPtr handleBr(llvm::BranchInst& br, size_t succIdx);
    ExprPtr handleSwitch(llvm::SwitchInst& swi, size_t succIdx);

private:
    Variable* getVariable(const llvm::Value* value);
    ExprPtr operand(const llvm::Value* value);

    ExprPtr asBool(ExprPtr operand);
    ExprPtr asInt(ExprPtr operand, unsigned bits);
    ExprPtr castResult(ExprPtr expr, const Type& type);

    ExprPtr integerCast(llvm::CastInst& cast, ExprPtr operand, unsigned width);

    Type& typeFromLLVMType(const llvm::Type* type);
    Type& typeFromLLVMType(const llvm::Value* value);

private:
    llvm::Function& mFunction;
    GazerContext& mContext;
    ValueToVariableMapT& mVariables;
    ExprBuilder* mExprBuilder;
    llvm::DenseMap<llvm::Value*, ExprPtr>& mEliminatedValues;
    //Variable& mStack;
    //Variable& mHeap;
};

}

#endif
