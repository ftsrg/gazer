#include "gazer/Automaton/Cfa.h"
#include "gazer/Automaton/CfaTransforms.h"
#include "gazer/Automaton/CallGraph.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Expr/ExprRewrite.h"
#include "gazer/Core/Expr/ExprBuilder.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/DenseSet.h>

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

    void transform();

private:
    void addUniqueErrorLocation();
    void inlineCallIntoRoot(CallTransition* call, llvm::Twine suffix);

private:
    Cfa* mRoot;
    CallGraph mCallGraph;
    llvm::SmallVector<CallTransition*, 8> mTailRecursiveCalls;
    Location* mError;
    Variable* mErrorFieldVariable;
    std::unique_ptr<ExprBuilder> mExprBuilder;
    unsigned mInlineCnt = 0;
};

} // end anonymous namespace

void RecursiveToCyclicTransformer::transform()
{
    this->addUniqueErrorLocation();

    for (auto& edge : mRoot->edges()) {
        if (auto call = llvm::dyn_cast<CallTransition>(edge.get())) {
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
    mRoot->clearDisconnectedElements();
}

void RecursiveToCyclicTransformer::inlineCallIntoRoot(CallTransition* call, llvm::Twine suffix)
{
    Cfa* callee = call->getCalledAutomaton();
    Location* before = call->getSource();
    Location* after  = call->getTarget();

    ExprRewrite rewrite(*mExprBuilder);
    llvm::DenseMap<Location*, Location*> locToLocMap;
    llvm::DenseMap<Variable*, Variable*> oldVarToNew;


    // Clone all local variables into the parent
    for (Variable& local : callee->locals()) {
        if (!callee->isOutput(&local)) {
            auto varname = (local.getName() + suffix).str();
            auto newLocal = mRoot->createLocal(varname, local.getType());
            oldVarToNew[&local] = newLocal;
            rewrite[&local] = newLocal->getRefExpr();
        }
    }
    
    // Clone input variables as well; we will insert an assign transition
    // with the initial values later.
    for (size_t i = 0; i < callee->getNumInputs(); ++i) {
        Variable* input = callee->getInput(i);
        if (!callee->isOutput(input)) {
            auto varname = (input->getName() + suffix).str();
            auto newInput = mRoot->createLocal(varname, input->getType());
            oldVarToNew[input] = newInput;
            rewrite[input] = newInput->getRefExpr();
        }
    }

    for (size_t i = 0; i < callee->getNumOutputs(); ++i) {
        Variable* output = callee->getOutput(i);
        auto newOutput = call->getOutputArgument(i).getVariable();
        oldVarToNew[output] = newOutput;
        rewrite[output] = call->getOutputArgument(i).getVariable()->getRefExpr();
    }

    // Insert all locations
    for (auto& origLoc : callee->nodes()) {
        auto newLoc = mRoot->createLocation();
        locToLocMap[&*origLoc] = newLoc;
        if (origLoc->isError()) {
            mRoot->createAssignTransition(newLoc, mError, mExprBuilder->True(), {
                { mErrorFieldVariable, callee->getErrorFieldExpr(origLoc.get()) }
            });
        }
    }

    // Clone the edges

    for (auto& origEdge : callee->edges()) {
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
                    Variable* input = callee->getInput(i);
                    recursiveInputArgs.push_back({
                        oldVarToNew[input],
                        nestedCall->getInputArgument(i)
                    });
                }

                // Create the assignment back-edge.
                mRoot->createAssignTransition(
                    source, locToLocMap[callee->getEntry()],
                    nestedCall->getGuard(), recursiveInputArgs
                );
            } else if (mCallGraph.isTailRecursive(nestedCall->getCalledAutomaton())) {
                // If the call is to another tail-recursive automaton, we add it
                // to the worklist.
                mTailRecursiveCalls.push_back(nestedCall);
            } else {
                // Otherwise we inline it as a regular call.
                ExprVector newArgs;
                std::vector<VariableAssignment> newOuts;

                std::transform(
                    nestedCall->input_begin(), nestedCall->input_end(),
                    std::back_inserter(newArgs),
                    [&rewrite](const ExprPtr& expr) { return rewrite.walk(expr); }
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

                mRoot->createCallTransition(
                    source, target,
                    rewrite.walk(nestedCall->getGuard()),
                    nestedCall->getCalledAutomaton(),
                    newArgs, newOuts
                );
            }
        }
    }
        
    std::vector<VariableAssignment> inputArgs;
    for (size_t i = 0; i < callee->getNumInputs(); ++i) {
        Variable* input = callee->getInput(i);
        inputArgs.push_back({
            oldVarToNew[input],
            call->getInputArgument(i)
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

void RecursiveToCyclicTransformer::addUniqueErrorLocation()
{
    auto& intTy = IntType::Get(mRoot->getParent().getContext());
    auto& ctx = mRoot->getParent().getContext();

    llvm::SmallVector<Location*, 1> errors;
    for (auto& loc : mRoot->nodes()) {
        if (loc->isError()) {
            errors.push_back(loc.get());
        }
    }
    
    mError = mRoot->createErrorLocation();
    Variable* mErrorFieldVariable = mRoot->createLocal("__gazer_error_field", intTy);

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

void gazer::TransformRecursiveToCyclic(Cfa* cfa)
{
    RecursiveToCyclicTransformer transformer(cfa);
    transformer.transform();
}
