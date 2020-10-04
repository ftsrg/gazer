//==-------------------------------------------------------------*- C++ -*--==//
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
#ifndef GAZER_TOOLS_GAZERTHETA_THETACFAGENERATOR_H
#define GAZER_TOOLS_GAZERTHETA_THETACFAGENERATOR_H

#include "gazer/Automaton/CallGraph.h"
#include "gazer/Automaton/Cfa.h"

#include <llvm/Support/raw_ostream.h>

#include <gazer/Core/LiteralExpr.h>

namespace llvm
{
    class Pass;
} // end namespace llvm

namespace gazer::theta
{

std::string printThetaExpr(const ExprPtr& expr);

std::string printThetaExpr(const ExprPtr& expr, std::function<std::string(Variable*)> variableNames);

llvm::SmallVector<ExprRef<gazer::ArrayLiteralExpr>, 1> collectArrayLiteralsThetaExpr(const ExprPtr& expr);

/// \brief Perform pre-processing steps required by theta on the input CFA.
///
/// This pass does the following transformations:
///     - Undefined values in expressions are replaced with previously havoc'd variables.
///     - Array literals are replaced with a appropriately constructed array variable.
void preprocessCfa(Cfa* cfa);

struct ThetaNameMapping
{
    llvm::StringMap<Location*> locations;
    llvm::StringMap<Variable*> variables;
    Location* errorLocation;
    Variable* errorFieldVariable;
    llvm::DenseMap<Location*, Location*> inlinedLocations;
    llvm::DenseMap<Variable*, Variable*> inlinedVariables;
};

class ThetaCfaGenerator
{
public:
    ThetaCfaGenerator(AutomataSystem& system)
        : mSystem(system), mCallGraph(system)
    {}

    void write(llvm::raw_ostream& os, ThetaNameMapping& names);

private:
    std::string validName(std::string name, std::function<bool(const std::string&)> isUnique);

private:
    AutomataSystem& mSystem;
    CallGraph mCallGraph;
    unsigned mTmpCount = 0;
};

llvm::Pass* createThetaCfaWriterPass(llvm::raw_ostream& os);

} // end namespace gazer::theta

#endif