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

// Cloning
//===----------------------------------------------------------------------===//
struct CfaCloneResult
{
    Cfa* clonedCfa;
    llvm::DenseMap<Location*, Location*> locToLocMap;
    llvm::DenseMap<Variable*, Variable*> varToVarMap;
};

/// Creates a clone of \p cfa and inserts it into its parent automaton system.
CfaCloneResult CloneCfa(Cfa& cfa, const std::string& nameSuffix = "_clone");

// Inlining
//===----------------------------------------------------------------------===//

struct CfaInlineResult
{
    llvm::DenseMap<Location*, Location*> locToLocMap;
    llvm::DenseMap<Transition*, Transition*> edgeToEdgeMap;
    llvm::DenseMap<Variable*, Variable*> oldVarToNew;
    std::vector<CallTransition*> newCalls;
    llvm::DenseMap<Variable*, Variable*> inlinedVariables;
    llvm::DenseMap<Location*, Location*> inlinedLocations;
};

/// Inlines the callee of a given call transition into the caller automaton and returns all the
/// necessary mappings to track the inlining result. Note that this function does *NOT* clean up
/// possibly removed components, you need to clean-up afterwards using `clearDisconnectedElements`.
CfaInlineResult InlineCall(
    CallTransition* call,
    Location* errorLoc,
    Variable* errorFieldVariable,
    const std::string& nameSuffix = "_inlined");


// Translate recursive automata into cyclic
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

} // namespace gazer

#endif