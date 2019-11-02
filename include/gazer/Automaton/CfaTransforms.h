//==- CfaTransforms.h - Cfa transformation utilities ------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
//
/// \file This file declares common CFA transformation functions.
//
//===----------------------------------------------------------------------===//
#ifndef GAZER_AUTOMATON_CFATRANSFORMS_H
#define GAZER_AUTOMATON_CFATRANSFORMS_H

#include "gazer/Automaton/Cfa.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/DenseMap.h>

namespace gazer
{

//===----------------------------------------------------------------------===//
/// Creates a clone of the given CFA with the given name.
/// Note that the clone shall be shallow one: automata called by the source
/// CFA shall be the same in the cloned one.
Cfa* CloneAutomaton(Cfa* cfa, llvm::StringRef name);


//===----------------------------------------------------------------------===//
struct RecursiveToCyclicResult
{
    Location* errorLocation = nullptr;
    Variable* errorFieldVariable = nullptr;
    llvm::DenseMap<Location*, Location*> inlinedLocations;
    llvm::DenseMap<Variable*, Variable*> inlinedVariables;
};

/// Transforms the given recursive CFA into a cyclic one, by inlining all
/// tail-recursive calls and adding latch edges.
/// Note that cyclic CFAs are non-canon, and should only be used if they are
/// transformed into the input format of a different verifier.
RecursiveToCyclicResult TransformRecursiveToCyclic(Cfa* cfa);

//===----------------------------------------------------------------------===//
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