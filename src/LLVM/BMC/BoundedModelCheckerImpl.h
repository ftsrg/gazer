#ifndef _GAZER_SRC_LLVM_BMC_BOUNDEDMODELCHECKERIMPL_H
#define _GAZER_SRC_LLVM_BMC_BOUNDEDMODELCHECKERIMPL_H

#include "gazer/LLVM/BMC/BMC.h"
#include "gazer/LLVM/Ir2Expr.h"

namespace gazer
{

class BoundedModelCheckerImpl
{
public:
    BoundedModelCheckerImpl(
        llvm::Function& function,
        TopologicalSort& topo,
        ExprBuilder* exprBuilder,
        SolverFactory& solverFactory,
        llvm::raw_ostream& os
    );

    using ProgramEncodeMapT = llvm::SmallDenseMap<llvm::BasicBlock*, ExprPtr, 1>;
    
    ProgramEncodeMapT encode();
    ExprPtr encodeEdge(llvm::BasicBlock* from, llvm::BasicBlock* to);

    BmcResult run();

private:
    // Basic stuff
    llvm::Function& mFunction;
    TopologicalSort& mTopo;
    SolverFactory& mSolverFactory;
    llvm::raw_ostream& mOS;
    // Symbols and mappings
    SymbolTable mSymbols;
    ExprBuilder* mExprBuilder;

    using VariableToValueMapT = llvm::DenseMap<Variable*, llvm::Value*>;
    VariableToValueMapT mVariableToValueMap;

    InstToExpr::ValueToVariableMapT mVariables;

    using FormulaCacheT = llvm::DenseMap<llvm::BasicBlock*, ExprPtr>;
    FormulaCacheT mFormulaCache;

    // Utils
    InstToExpr mIr2Expr;
};

}

#endif
