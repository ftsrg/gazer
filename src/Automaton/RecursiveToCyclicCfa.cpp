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
#include "gazer/Automaton/Cfa.h"
#include "gazer/Automaton/CfaTransforms.h"
#include "gazer/Automaton/CallGraph.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Expr/ExprRewrite.h"
#include "gazer/Core/Expr/ExprBuilder.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/Support/raw_ostream.h>

using namespace gazer;

namespace
{

class RecursiveToCyclicTransformer
{
public:
    RecursiveToCyclicTransformer(Cfa* cfa)
        : mRoot(cfa), mCallGraph(cfa->getParent()),
        mExprBuilder(CreateExprBuilder(cfa->getParent().getContext()))
    {}

    RecursiveToCyclicResult transform();

private:
    void inlineCallIntoRoot(CallTransition* call, llvm::Twine suffix);
    void createErrorTransition(Location* from, ExprPtr errorFieldExpr);
    void createDummyErrorTransition();

private:
    Cfa* mRoot;
    CallGraph mCallGraph;
    llvm::SmallVector<CallTransition*, 8> mTailRecursiveCalls;
    Location* mError = nullptr;
    Variable* mErrorFieldVariable = nullptr;
    llvm::DenseMap<Location*, Location*> mInlinedLocations;
    llvm::DenseMap<Variable*, Variable*> mInlinedVariables;
    std::unique_ptr<ExprBuilder> mExprBuilder;
    unsigned mInlineCnt = 0;
};

} // end anonymous namespace

RecursiveToCyclicResult RecursiveToCyclicTransformer::transform()
{
    mError = mRoot->createErrorLocation();

    // Connect error location
    for (Location* loc : mRoot->nodes()) {
        if (loc->isError() && loc != mError) {
            this->createErrorTransition(loc, mRoot->getErrorFieldExpr(loc));
        }
    }

    // Collect tail-recursive calls
    for (Transition* edge : mRoot->edges()) {
        if (auto call = llvm::dyn_cast<CallTransition>(edge)) {
            if (mCallGraph.isTailRecursive(call->getCalledAutomaton())) {
                mTailRecursiveCalls.push_back(call);
            }
        }
    }

    // Inline each tail-recursive call into the main automaton.
    while (!mTailRecursiveCalls.empty()) {
        CallTransition* call = mTailRecursiveCalls.back();
        mTailRecursiveCalls.pop_back();

        this->inlineCallIntoRoot(call, "_inlined" + llvm::Twine(mInlineCnt++));
    }

    if(mErrorFieldVariable == nullptr) {
        // If there are no error locations, create one to use as goal
        this->createDummyErrorTransition();
    }

    mRoot->clearDisconnectedElements();

    // Calculate the new call graph and remove unneeded automata.
    //mCallGraph = CallGraph(mRoot->getParent());

    return {
        mError,
        mErrorFieldVariable,
        std::move(mInlinedLocations),
        std::move(mInlinedVariables)
    };
}

void RecursiveToCyclicTransformer::inlineCallIntoRoot(CallTransition* call, llvm::Twine suffix)
{
    Cfa* callee = call->getCalledAutomaton();
    Location* before = call->getSource();
    Location* after  = call->getTarget();

    VariableExprRewrite rewrite(*mExprBuilder);
    llvm::DenseMap<Location*, Location*> locToLocMap;
    llvm::DenseMap<Variable*, Variable*> oldVarToNew;

    // Clone all local variables into the parent
    for (Variable& local : callee->locals()) {
        if (!callee->isOutput(&local)) {
            auto varname = (local.getName() + suffix).str();
            auto newLocal = mRoot->createLocal(varname, local.getType());
            oldVarToNew[&local] = newLocal;
            mInlinedVariables[newLocal] = &local;
            rewrite[&local] = newLocal->getRefExpr();
        }
    }

    // Clone input variables as well; we will insert an assign transition
    // with the initial values later.
    std::vector<Variable*> inputTemporaries;
    for (Variable& input : callee->inputs()) {
        if (!callee->isOutput(&input)) {
            auto varname = (input.getName() + suffix).str();
            auto newInput = mRoot->createLocal(varname, input.getType());
            oldVarToNew[&input] = newInput;
            mInlinedVariables[newInput] = &input;
            //rewrite[input] = call->getInputArgument(i);
            rewrite[&input] = newInput->getRefExpr();

            auto val = mRoot->createLocal(varname+"_", input.getType());
            inputTemporaries.emplace_back(val);
        }
    }

    for (Variable& output : callee->outputs()) {
        auto argument = call->getOutputArgument(output);
        assert(argument.has_value() && "Every callee output should be assigned in a call transition!");

        auto newOutput = argument->getVariable();
        oldVarToNew[&output] = newOutput;
        mInlinedVariables[newOutput] = &output;
        rewrite[&output] = newOutput->getRefExpr();
    }

    // Insert all locations
    for (Location* origLoc : callee->nodes()) {
        auto newLoc = mRoot->createLocation();
        locToLocMap[&*origLoc] = newLoc;
        mInlinedLocations[newLoc] = origLoc;
        if (origLoc->isError()) {
            this->createErrorTransition(newLoc, callee->getErrorFieldExpr(origLoc));
        }
    }

    // Clone the edges
    for (Transition* origEdge : callee->edges()) {
        Location* source = locToLocMap[origEdge->getSource()];
        Location* target = locToLocMap[origEdge->getTarget()];

        if (auto assign = llvm::dyn_cast<AssignTransition>(&*origEdge)) {
            std::vector<VariableAssignment> newAssigns;
            std::transform(
                assign->begin(), assign->end(), std::back_inserter(newAssigns),
                [&oldVarToNew, &rewrite] (const VariableAssignment& origAssign) {
                    return VariableAssignment {
                        oldVarToNew[origAssign.getVariable()],
                        rewrite.walk(origAssign.getValue())
                    };
                }
            );

            mRoot->createAssignTransition(
                source, target, rewrite.walk(assign->getGuard()), newAssigns
            );
        } else if (auto nestedCall = llvm::dyn_cast<CallTransition>(&*origEdge)) {
            if (nestedCall->getCalledAutomaton() == callee) {
                // This is where the magic happens: if we are calling this
                // same automaton, replace the recursive call with a back-edge
                // to the entry.
                std::vector<VariableAssignment> recursiveInputArgs;
                for (size_t i = 0; i < callee->getNumInputs(); ++i) {
                    // result variable is different then original inputs #46
                    // to simulate parallel assignments
                    Variable* input = inputTemporaries[i];
                    Variable* realInput = callee->getInput(i);

                    auto variable = input;
                    auto value = rewrite.walk(nestedCall->getInputArgument(*realInput)->getValue());

                    if (variable->getRefExpr() != value) {
                        // Do not add unneeded assignments (X := X).
                        recursiveInputArgs.push_back({
                                                         variable,
                                                         value
                                                     });
                    }
                }

                for (size_t i = 0; i < callee->getNumInputs(); ++i) {
                    Variable* inputTemp = inputTemporaries[i];
                    Variable* realInput = callee->getInput(i);

                    auto variable = oldVarToNew[realInput];
                    auto value = inputTemp->getRefExpr();

                    recursiveInputArgs.push_back({
                                                     variable,
                                                     value
                                                 });
                }

                // Create the assignment back-edge.
                mRoot->createAssignTransition(
                    source, locToLocMap[callee->getEntry()],
                    nestedCall->getGuard(), recursiveInputArgs
                );
            } else {
                // Inline it as a normal call.
                std::vector<VariableAssignment> newArgs;
                std::vector<VariableAssignment> newOuts;

                std::transform(
                    nestedCall->input_begin(), nestedCall->input_end(),
                    std::back_inserter(newArgs),
                    [&rewrite](const VariableAssignment& assign) {
                        return VariableAssignment{assign.getVariable(), rewrite.walk(assign.getValue())};
                    }
                );
                std::transform(
                    nestedCall->output_begin(), nestedCall->output_end(),
                    std::back_inserter(newOuts),
                    [&oldVarToNew](const VariableAssignment& origAssign) {
                        Variable* newVar = oldVarToNew.lookup(origAssign.getVariable());
                        assert(newVar != nullptr
                            && "All variables should be present in the variable map!");

                        return VariableAssignment{
                            newVar,
                            origAssign.getValue()
                        };
                    }
                );

                auto newCall = mRoot->createCallTransition(
                    source, target,
                    rewrite.walk(nestedCall->getGuard()),
                    nestedCall->getCalledAutomaton(),
                    newArgs, newOuts
                );

                if (mCallGraph.isTailRecursive(nestedCall->getCalledAutomaton())) {
                    // If the call is to another tail-recursive automaton, we add it
                    // to the worklist.
                    mTailRecursiveCalls.push_back(newCall);
                }
            }
        }
    }
        
    std::vector<VariableAssignment> inputArgs;
    for (size_t i = 0; i < callee->getNumInputs(); ++i) {
        Variable* input = callee->getInput(i);
        inputArgs.push_back({
            oldVarToNew[input],
            call->getInputArgument(*input)->getValue()
        });
    }

    // We set the input variables to their initial values on a transition
    // between 'before' and the entry of the called CFA.
    mRoot->createAssignTransition(
        before, locToLocMap[callee->getEntry()], call->getGuard(), inputArgs
    );

    mRoot->createAssignTransition(
        locToLocMap[callee->getExit()], after , mExprBuilder->True()
    );

    // Remove the original call edge
    mRoot->disconnectEdge(call);
}

void RecursiveToCyclicTransformer::createErrorTransition(Location *from, ExprPtr errorFieldExpr)
{
    assert(mError != nullptr);
    assert(mErrorFieldVariable == nullptr || mErrorFieldVariable->getType() == errorFieldExpr->getType());

    if(mErrorFieldVariable == nullptr) {
        mErrorFieldVariable = mRoot->createLocal("__gazer_error_field", errorFieldExpr->getType());
    }

    mRoot->createAssignTransition(from, mError, BoolLiteralExpr::True(mRoot->getParent().getContext()), {
        VariableAssignment { mErrorFieldVariable, errorFieldExpr }
    });
}

void RecursiveToCyclicTransformer::createDummyErrorTransition()
{
    assert(mError != nullptr);
    assert(mErrorFieldVariable == nullptr);

    auto& intTy = IntType::Get(mRoot->getParent().getContext());
    auto& ctx = mRoot->getParent().getContext();

    // A dummy error location will be used as a goal as there are no error locations in the automaton
    mErrorFieldVariable = mRoot->createLocal("__gazer_error_field", intTy);
    mRoot->createAssignTransition(mRoot->getEntry(), mError, BoolLiteralExpr::False(ctx), {
        VariableAssignment{ mErrorFieldVariable, IntLiteralExpr::Get(intTy, 0) }
    });
}

RecursiveToCyclicResult gazer::TransformRecursiveToCyclic(Cfa* cfa)
{
    RecursiveToCyclicTransformer transformer(cfa);
    return transformer.transform();
}
