#ifndef _GAZER_CORE_UTILS_CFAUTILS_H
#define _GAZER_CORE_UTILS_CFAUTILS_H

#include "gazer/Core/Automaton.h"
#include "gazer/Core/SymbolTable.h"
#include "gazer/Core/ExprVisitor.h"

#include <llvm/ADT/DenseMap.h>

namespace gazer
{

class VariableIndex;

/**
 * Creates a visitor objects suitable for rewriting
 * variables and variable references.
 */
std::unique_ptr<ExprVisitor<ExprPtr>>
createVariableRewriteVisitor(VariableIndex* vi);

class VariableIndex
{
public:
    static constexpr unsigned DefaultIndex = 0;

    VariableIndex(SymbolTable& symbols);

    /**
     * Increments the index value of the given variable.
     */
    Variable* increment(const Variable* variable);

    /**
     * Returns the highest-index version of the given variable.
     */
    Variable* current(const Variable* variable);

    /**
     * Returns the current index of the given variable.
     */
    std::optional<unsigned> find(Variable* variable);

private:
    SymbolTable mSymbols;
    llvm::DenseMap<Variable*, unsigned> mMap;
};

/**
 * Transforms a range of CfaEdges into booleans expressions.
 * 
 * @param st The initial symbol table containing all variables.
 * @param begin Beginning of the iterator range.
 * @param end End of the iterator range.
 * @param out An OutputIterator for expression inseration.
 */
template<class InputIterator, class OutputIterator>
void PathToExprs(SymbolTable& st, InputIterator begin, InputIterator end, OutputIterator out)
{
    VariableIndex vi(st);
    auto visitor = createVariableRewriteVisitor(&vi);
    
    for (InputIterator it = begin; it != end; ++it) {
        CfaEdge* edge = *it;
        if (edge->isAssume()) {
            auto assume = llvm::dyn_cast<AssumeEdge>(edge);
            *(out++) = visitor->visit(assume->getCondition());
        } else if (edge->isAssign()) {
            auto assignEdge = llvm::dyn_cast<AssignEdge>(edge);
            for (auto& assignment : assignEdge->assignments()) {
                Variable* variable = &(assignment.variable);
                auto rhs = visitor->visit(assignment.expr);
                Variable* newVar = vi.increment(variable);
                
                ExprPtr assignExpr = EqExpr::Create(newVar->getRefExpr(), rhs);
                *(out++) = assignExpr;
            }
        } else if (edge->isHavoc()) {
            auto havocEdge = llvm::dyn_cast<HavocEdge>(edge);
            for (Variable* nondetVar : havocEdge->vars()) {
                vi.increment(nondetVar);
            }
        } else if (edge->isSkip()) {
            // We do nothing here
        } else {
            llvm_unreachable("Unhandled edge type.");
        }
    }
}

std::unique_ptr<ExprVisitor<ExprPtr>> createVariableRewriteVisitor();

} // end namespace gazer

#endif
