#ifndef GAZER_AUTOMATON_CFATRANSFORMS_H
#define GAZER_AUTOMATON_CFATRANSFORMS_H

#include "gazer/Automaton/Cfa.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/DenseMap.h>

namespace gazer
{

/// Creates a clone of the given CFA with the given name.
/// Note that the clone shall be shallow one: automata called by the source
/// CFA shall be the same in the cloned one.
Cfa* CloneAutomaton(Cfa* cfa, llvm::StringRef name);

/// Transforms the given recursive CFA into a cyclic one, by inlining all
/// tail-recursive calls and adding latch edges.
/// Note that cyclic CFAs are non-canon, and should only be used if they are
/// transformed into the input format of a different verifier.
void TransformRecursiveToCyclic(Cfa* cfa);

struct InlineResult
{
    llvm::DenseMap<Variable*, Variable*> VariableMap;
    llvm::DenseMap<Location*, Location*> InlinedLocations;
    llvm::SmallVector<Location*, 8> NewErrors;
    std::vector<Location*> NewLocations;
};

//void InlineCall(CallTransition* call, InlineResult& result, ExprBuilder& exprBuilder, llvm::Twine suffix = "_inlined");

}

#endif