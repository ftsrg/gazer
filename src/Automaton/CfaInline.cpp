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
#include "gazer/Automaton/CfaTransforms.h"

#include "gazer/Core/Expr/ExprRewrite.h"

#include <llvm/ADT/Twine.h>

using namespace gazer;

CfaInlineResult gazer::InlineCall(
    CallTransition* call,
    Location* errorLoc,
    Variable* errorFieldVariable,
    const std::string& suffix)
{
    assert(call->getSource() != nullptr && call->getTarget() != nullptr);

    Cfa* callee = call->getCalledAutomaton();
    Cfa* target = call->getSource()->getAutomaton();

    Location* before = call->getSource();
    Location* after = call->getTarget();

    auto exprBuilder = CreateExprBuilder(target->getParent().getContext());
    VariableExprRewrite rewrite(*exprBuilder);

    CfaInlineResult result;

    // Clone local variables into the caller
    for (Variable* local : callee->locals()) {
        if (!callee->isOutput(local)) {
            auto varname = local->getName() + suffix;
            auto* newLocal = target->createLocal(varname, local->getType());
            result.oldVarToNew[local] = newLocal;
            result.inlinedVariables[newLocal] = local;
            rewrite[local] = newLocal->getRefExpr();
        }
    }

    // Clone input variables as well; we will insert an assign transition
    // with the initial values later.
    std::vector<Variable*> inputTemporaries;
    for (Variable* input : callee->inputs()) {
        if (!callee->isOutput(input)) {
            auto varname = input->getName() + suffix;
            auto* newInput = target->createLocal(varname, input->getType());
            result.oldVarToNew[input] = newInput;
            result.inlinedVariables[newInput] = input;
            //rewrite[input] = call->getInputArgument(i);
            rewrite[input] = newInput->getRefExpr();

            auto* val = target->createLocal(varname+"_", input->getType());
            inputTemporaries.emplace_back(val);
        }
    }

    for (Variable* output : callee->outputs()) {
        auto argument = call->getOutputArgument(*output);
        assert(argument.has_value() && "Every callee output should be assigned in a call transition!");

        auto* newOutput = argument->getVariable();
        result.oldVarToNew[output] = newOutput;
        result.inlinedVariables[newOutput] = output;
        rewrite[output] = newOutput->getRefExpr();
    }

    // Insert all locations
    for (Location* origLoc : callee->nodes()) {
        auto* newLoc = target->createLocation();
        result.locToLocMap[origLoc] = newLoc;
        result.inlinedLocations[newLoc] = origLoc;
        if (origLoc->isError()) {
            target->createAssignTransition(newLoc, errorLoc, exprBuilder->True(), {
                { errorFieldVariable, callee->getErrorFieldExpr(origLoc) }
            });
        }
    }

    for (auto* origEdge : callee->edges()) {
        Transition* newEdge;
        Location* from = result.locToLocMap[origEdge->getSource()];
        Location* to = result.locToLocMap[origEdge->getTarget()];

        if (auto* assign = llvm::dyn_cast<AssignTransition>(origEdge)) {
            // Transform the assignments of this edge to use the new variables.
            std::vector<VariableAssignment> newAssigns;
            std::transform(
                assign->begin(), assign->end(), std::back_inserter(newAssigns),
                [&result, &rewrite] (const VariableAssignment& origAssign) {
                  return VariableAssignment {
                      result.oldVarToNew[origAssign.getVariable()],
                      rewrite.rewrite(origAssign.getValue())
                  };
                }
            );

            newEdge = target->createAssignTransition(
                from, to, rewrite.rewrite(assign->getGuard()), newAssigns
            );
        } else if (auto* nestedCall = llvm::dyn_cast<CallTransition>(origEdge)) {
            std::vector<VariableAssignment> newArgs;
            std::vector<VariableAssignment> newOuts;

            std::transform(
                nestedCall->input_begin(), nestedCall->input_end(),
                std::back_inserter(newArgs),
                [&rewrite](const VariableAssignment& a) {
                  return VariableAssignment{a.getVariable(), rewrite.rewrite(a.getValue())};
                }
            );
            std::transform(
                nestedCall->output_begin(), nestedCall->output_end(),
                std::back_inserter(newOuts),
                [&result](const VariableAssignment& origAssign) {
                  Variable* newVar = result.oldVarToNew.lookup(origAssign.getVariable());
                  assert(newVar != nullptr && "All variables should be present in the variable map!");

                  return VariableAssignment{
                      newVar,
                      origAssign.getValue()
                  };
                }
            );

            auto* callEdge = target->createCallTransition(
                from, to,
                rewrite.rewrite(nestedCall->getGuard()),
                nestedCall->getCalledAutomaton(),
                newArgs,
                newOuts
            );

            newEdge = callEdge;
            result.newCalls.push_back(callEdge);
        } else {
            llvm_unreachable("Unknown transition kind!");
        }

        result.edgeToEdgeMap[origEdge] = newEdge;
    }

    std::vector<VariableAssignment> inputAssigns;
    for (const auto& input : call->inputs()) {
        VariableAssignment inputAssignment(result.oldVarToNew[input.getVariable()], input.getValue());
        inputAssigns.push_back(inputAssignment);
    }

    target->createAssignTransition(before, result.locToLocMap[callee->getEntry()], call->getGuard(), inputAssigns);
    target->createAssignTransition(result.locToLocMap[callee->getExit()], after , exprBuilder->True());
    target->disconnectEdge(call);

    return result;
}
