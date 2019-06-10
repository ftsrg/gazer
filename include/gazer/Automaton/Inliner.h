#ifndef GAZER_AUTOMATON_INLINER_H
#define GAZER_AUTOMATON_INLINER_H

#include "gazer/Automaton/Cfa.h"

namespace gazer
{
    
    void InlineCall(
        CallTransition* call,
        llvm::DenseMap<Variable*, Variable*>& vmap,
        llvm::StringRef suffix = ""
    );

}

#endif