#ifndef GAZER_CORE_EXPR_EXPRPROPAGATOR_H
#define GAZER_CORE_EXPR_EXPRPROPAGATOR_H

#include "gazer/Core/Expr.h"

#include <llvm/ADT/DenseMap.h>

namespace gazer
{

class PropagationTable
{
    struct PropagationTableEntry
    {
        Variable* variable;
        llvm::SmallVector<ExprPtr, 1> exprs;

        PropagationTableEntry()
            : variable(nullptr), exprs()
        {}

        PropagationTableEntry(Variable* variable, ExprPtr expr)
            : variable(variable), exprs(1, expr)
        {}
    };

    using TableT = llvm::DenseMap<Variable*, PropagationTableEntry>;
public:
    void put(Variable* variable, ExprPtr expr)
    {
        auto entry = mTable[variable];
        entry.variable = variable;
        entry.exprs.push_back(expr);
    }

    llvm::iterator_range<llvm::SmallVector<ExprPtr, 1>::iterator> get(Variable* variable)
    {
        auto& exprs = mTable[variable].exprs;
        return llvm::make_range(exprs.begin(), exprs.end());
    }

private:
    TableT mTable;
};

ExprPtr PropagateExpression(ExprPtr expr, PropagationTable& propTable, unsigned depth = 1);

}

#endif
