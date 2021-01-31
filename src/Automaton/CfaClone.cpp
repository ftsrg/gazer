//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2021 Contributors to the Gazer project
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
#include "gazer/Automaton/CfaTransforms.h"

#include "gazer/Core/Expr/ExprRewrite.h"

using namespace gazer;

CfaCloneResult gazer::CloneCfa(Cfa& cfa, const std::string& nameSuffix)
{
    AutomataSystem& system = cfa.getParent();
    Cfa* newCfa = system.createCfa(cfa.getName().str() + nameSuffix);

    llvm::DenseMap<Location*, Location*> locToLocMap(cfa.node_size());
    llvm::DenseMap<Variable*, Variable*> variablesMap(cfa.getNumLocals());

    for (Location* loc : cfa.nodes()) {
        Location* newLoc;
        if (loc->isError()) {
            newLoc = newCfa->createErrorLocation();
        } else {
            newLoc = newCfa->createLocation();
        }
        locToLocMap[loc] = newLoc;
    }
    locToLocMap[cfa.getEntry()] = newCfa->getEntry();
    locToLocMap[cfa.getExit()] = newCfa->getExit();

    for (Variable& variable : cfa.locals()) {
        Variable* newVar = newCfa->createLocal(variable.getName(), variable.getType());
        variablesMap[&variable] = newVar;
    }

    // Add inputs and outputs - order matters here.
    for (unsigned i = 0; i < cfa.getNumInputs(); ++i) {
        Variable* variable = cfa.getInput(i);
        Variable* newVar = newCfa->createInput(variable->getName(), variable->getType());
        variablesMap[variable] = newVar;
    }

    for (unsigned i = 0; i < cfa.getNumOutputs(); ++i) {
        Variable* variable = cfa.getOutput(i);
        Variable* newVar = variablesMap[variable];
        newCfa->addOutput(newVar);
    }

    auto exprBuilder = CreateExprBuilder(system.getContext());
    VariableExprRewrite exprRewrite(*exprBuilder);
    for (auto& [oldVar, newVar] : variablesMap) {
        exprRewrite[oldVar] = newVar->getRefExpr();
    }

    for (Transition* edge : cfa.edges()) {
        if (auto* assign = llvm::dyn_cast<AssignTransition>(edge)) {
            std::vector<VariableAssignment> newAssigns;
            newAssigns.reserve(assign->getNumAssignments());
            for (const VariableAssignment& va : *assign) {
                newAssigns.emplace_back(variablesMap[va.getVariable()], exprRewrite.rewrite(va.getValue()));
            }

            newCfa->createAssignTransition(
                locToLocMap[assign->getSource()],
                locToLocMap[assign->getTarget()],
                exprRewrite.rewrite(assign->getGuard()),
                newAssigns
            );
        } else if (auto* call = llvm::dyn_cast<CallTransition>(edge)) {
            Cfa* newCallee = call->getCalledAutomaton() == &cfa ? newCfa : call->getCalledAutomaton();

            std::vector<VariableAssignment> newInputs;
            newInputs.reserve(call->getNumInputs());
            for (const VariableAssignment& va : call->inputs()) {
                newInputs.emplace_back(va.getVariable(), exprRewrite.rewrite(va.getValue()));
            }

            std::vector<VariableAssignment> newOutputs;
            newOutputs.reserve(call->getNumOutputs());
            for (const VariableAssignment& va : call->outputs()) {
                newOutputs.emplace_back(variablesMap[va.getVariable()], va.getValue());
            }

            newCfa->createCallTransition(
                locToLocMap[call->getSource()],
                locToLocMap[call->getTarget()],
                exprRewrite.rewrite(call->getGuard()),
                newCallee,
                newInputs,
                newOutputs
            );
        } else {
            llvm_unreachable("Unknown transition kind!");
        }
    }

    // Copy error codes
    for (const auto& [loc, expr] : cfa.errors()) {
        newCfa->addErrorCode(locToLocMap[loc], exprRewrite.rewrite(expr));
    }

    return CfaCloneResult{newCfa, std::move(locToLocMap), std::move(variablesMap)};
}
