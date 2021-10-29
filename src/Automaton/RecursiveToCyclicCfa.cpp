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
    void addUniqueErrorLocation();
    void inlineCallIntoRoot(CallTransition* call, llvm::Twine suffix);

private:
    Cfa* mRoot;
    CallGraph mCallGraph;
    llvm::SmallVector<CallTransition*, 8> mTailRecursiveCalls;
    Location* mError;
    Variable* mErrorFieldVariable;;
    llvm::DenseMap<Location*, Location*> mInlinedLocations;
    llvm::DenseMap<Variable*, Variable*> mInlinedVariables;
    std::unique_ptr<ExprBuilder> mExprBuilder;
    unsigned mInlineCnt = 0;
};

} // end anonymous namespace

RecursiveToCyclicResult RecursiveToCyclicTransformer::transform()
{
    this->addUniqueErrorLocation();

    for (auto* call : classof_range<CallTransition>(mRoot->edges())) {
        if (mCallGraph.isTailRecursive(call->getCalledAutomaton())) {
            mTailRecursiveCalls.push_back(call);
        }
    }

    // Inline each tail-recursive call into the main automaton.
    while (!mTailRecursiveCalls.empty()) {
        CallTransition* call = mTailRecursiveCalls.back();
        mTailRecursiveCalls.pop_back();

        this->inlineCallIntoRoot(call, "_inlined" + llvm::Twine(mInlineCnt++));
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

    CfaInlineResult inlineResult = InlineCall(call, mError, mErrorFieldVariable, suffix.str());

    // TODO: This is super not-nice: we are copying all the maps from the rewrite object that we
    //  have to re-create after the inlining.
    VariableExprRewrite rewrite(*mExprBuilder);
    llvm::for_each(inlineResult.oldVarToNew, [&rewrite](auto& entry) {
        rewrite[entry.first] = entry.second->getRefExpr();
    });

    // See the newly inserted calls: if it is the same automaton (callee is guaranteed to be
    // tail-recursive at this point), replace the call edge to the final location with a back-edge
    // to the entry location.
    for (CallTransition* newCall : inlineResult.newCalls) {
        if (newCall->getCalledAutomaton() != callee) {
            if (mCallGraph.isTailRecursive(newCall->getCalledAutomaton())) {
                mTailRecursiveCalls.emplace_back(newCall);
            }
            continue;
        }

        std::vector<VariableAssignment> recursiveInputArgs;
        std::vector<Variable*> inputTemporaries;
        for (size_t i = 0; i < callee->getNumInputs(); ++i) {
            // result variable is different then original inputs #46
            // to simulate parallel assignments
            Variable* realInput = callee->getInput(i);
            Variable* tempInput = mRoot->createLocal(realInput->getName() + "_temp", realInput->getType());

            auto value = rewrite.rewrite(newCall->getInputArgument(*realInput)->getValue());

            if (tempInput->getRefExpr() != value) {
                // Do not add unneeded assignments (X := X).
                recursiveInputArgs.emplace_back( tempInput, value );
            }

            inputTemporaries.emplace_back(tempInput);
        }

        for (size_t i = 0; i < callee->getNumInputs(); ++i) {
            Variable* inputTemp = inputTemporaries[i];
            Variable* realInput = callee->getInput(i);

            Variable* variable = inlineResult.oldVarToNew[realInput];
            ExprPtr value = inputTemp->getRefExpr();
            recursiveInputArgs.emplace_back( variable, value );
        }

        // Create the assignment back-edge.
        mRoot->createAssignTransition(
            newCall->getSource(), inlineResult.locToLocMap[callee->getEntry()],
            newCall->getGuard(), recursiveInputArgs
        );
        mRoot->disconnectEdge(newCall);
    }

    mRoot->clearDisconnectedElements();
}

void RecursiveToCyclicTransformer::addUniqueErrorLocation()
{
    auto& intTy = IntType::Get(mRoot->getParent().getContext());
    auto& ctx = mRoot->getParent().getContext();

    llvm::SmallVector<Location*, 1> errors;
    for (Location* loc : mRoot->nodes()) {
        if (loc->isError()) {
            errors.push_back(loc);
        }
    }
    
    mError = mRoot->createErrorLocation();
    mErrorFieldVariable = mRoot->createLocal("__gazer_error_field", intTy);

    if (errors.empty()) {
        // If there are no error locations in the main automaton, they might still exist in a called CFA.
        // A dummy error location will be used as a goal.
        mRoot->createAssignTransition(mRoot->getEntry(), mError, BoolLiteralExpr::False(ctx), {
            VariableAssignment{ mErrorFieldVariable, IntLiteralExpr::Get(intTy, 0) }
        });        
    } else {
        // The error location will be directly reachable from already existing error locations.
        for (Location* err : errors) {
            auto errorExpr = mRoot->getErrorFieldExpr(err);

            assert(errorExpr->getType().isIntType() && "Error expression must be arithmetic integers in the theta backend!");

            mRoot->createAssignTransition(err, mError, BoolLiteralExpr::True(ctx), {
                VariableAssignment { mErrorFieldVariable, errorExpr }
            });
        }
    }
}

RecursiveToCyclicResult gazer::TransformRecursiveToCyclic(Cfa* cfa)
{
    RecursiveToCyclicTransformer transformer(cfa);
    return transformer.transform();
}
