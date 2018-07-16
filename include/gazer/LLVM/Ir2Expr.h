#ifndef _GAZER_LLVM_IR2EXPR_H
#define _GAZER_LLVM_IR2EXPR_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/SymbolTable.h"
#include "gazer/Core/Utils/ExprBuilder.h"

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
        SymbolTable& symbols,
        ExprBuilder* builder,
        llvm::DenseMap<const Variable*, llvm::Value*>* variableToValueMap = nullptr
    );

    ExprPtr transform(llvm::Instruction& inst, size_t succIdx, llvm::BasicBlock* pred = nullptr);
    ExprPtr transform(llvm::Instruction& inst);

    ExprBuilder* getBuilder() const { return mExprBuilder; }

    const ValueToVariableMapT& getVariableMap() const { return mVariables; }

public:
    ExprPtr visitBinaryOperator(llvm::BinaryOperator &binop);
    ExprPtr visitSelectInst(llvm::SelectInst& select);
    ExprPtr visitICmpInst(llvm::ICmpInst& icmp);
    ExprPtr visitCastInst(llvm::CastInst& cast);
    ExprPtr visitCallInst(llvm::CallInst& call);
    
    ExprPtr handlePHINode(llvm::PHINode& phi, llvm::BasicBlock* pred);
    ExprPtr handleBr(llvm::BranchInst& br, size_t succIdx);
    ExprPtr handleSwitch(llvm::SwitchInst& swi, size_t succIdx);

private:
    Variable* getVariable(const llvm::Value* value);
    ExprPtr operand(const llvm::Value* value);

    ExprPtr asBool(ExprPtr operand);
    ExprPtr asInt(ExprPtr operand, unsigned bits);
    ExprPtr castResult(ExprPtr expr, const Type& type);

private:
    llvm::Function& mFunction;
    SymbolTable& mSymbols;
    ValueToVariableMapT mVariables;
    ExprBuilder* mExprBuilder;
};

}

#endif
