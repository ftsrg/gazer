#include "gazer/Verifier/BoundedModelChecker.h"

#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/Solver/Solver.h"
#include "gazer/Automaton/Cfa.h"
#include "gazer/Trace/SafetyResult.h"
#include "gazer/Core/Expr/ExprRewrite.h"

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/ADT/DenseSet.h>

namespace gazer
{

llvm::cl::opt<unsigned> MaxBound("bound", llvm::cl::desc("Maximum iterations for the bounded model checker."));
llvm::cl::opt<bool> VerifierDebug("debug-verif", llvm::cl::desc("Print verifier debug info"));

class BoundedModelChecker
{
    struct VcCell
    {
        ExprPtr expr;

        VcCell(ExprPtr expr = nullptr)
            : expr(expr)
        {}

        VcCell(const VcCell&) = default;
        VcCell& operator=(const VcCell&) = default;
    };

    struct CallInfo
    {
        ExprPtr overApprox = nullptr;
        unsigned cost = 0;
    };
public:
    BoundedModelChecker(
        AutomataSystem& system,
        ExprBuilder& builder,
        SolverFactory& solverFactory
    ) : mSystem(system), mExprBuilder(builder), mSolverFactory(solverFactory)
    {
        // TODO: Make this more flexible
        mRoot = mSystem.getAutomatonByName("main");

        size_t size = mRoot->getNumLocations();

        // Fill the initial VC vector with False
        mVC.resize(size);

        std::fill(mVC.begin(), mVC.end(), VcCell{mExprBuilder.False()});

        // Create the topological sort
        auto poBegin = llvm::po_begin(mRoot->getEntry());
        auto poEnd = llvm::po_end(mRoot->getEntry());
        mTopo.insert(mTopo.end(), poBegin, poEnd);
        std::reverse(mTopo.begin(), mTopo.end());

        for (size_t i = 0; i < size; ++i) {
            mLocNumbers[mTopo[i]] = i;
        }

        // Set the verification goal - a single error location.
        llvm::SmallVector<Location*, 1> errors;
        for (auto& loc : mRoot->nodes()) {
            if (loc->isError()) {
                errors.push_back(loc.get());
            }
        }
        
        if (errors.size() == 0) {
            // If there are no error locations in the main automaton, they might still exist in a called CFA.
            // Create a dummy error location which we will use as a goal.
            mError = mRoot->createErrorLocation();
            mRoot->createAssignTransition(mTopo.back(), mError, mExprBuilder.False());
            mLocNumbers[mError] = mTopo.size();
            mTopo.push_back(mError);
        } else if (errors.size() == 1) {
            // We have a single error location, let that be the verification goal.
            mError = errors[0];
        } else {
            // Create an error location which will be directly reachable from each already existing error locations.
            // This one error location will be used as the goal.
            mError = mRoot->createErrorLocation();
            for (Location* err : errors) {
                mRoot->createAssignTransition(err, mError, mExprBuilder.True());
            }
            mLocNumbers[mError] = mTopo.size();
            mTopo.push_back(mError);
        }

        // Insert initial call approximations.
        for (auto& edge : mRoot->edges()) {
            if (auto call = llvm::dyn_cast<CallTransition>(edge.get())) {
                mCalls[call].overApprox = mExprBuilder.False();
                mCalls[call].cost = 1;
            }
        }
    }

    std::unique_ptr<SafetyResult> check();

private:
    void updateVC(size_t startIdx);
    void inlineCallIntoRoot(
        CallTransition* call,
        llvm::DenseMap<Variable*, Variable*>& vmap,
        llvm::Twine suffix
    );

private:
    AutomataSystem& mSystem;
    ExprBuilder& mExprBuilder;
    SolverFactory& mSolverFactory;

    Cfa* mRoot;
    Location* mError;

    std::vector<VcCell> mVC;
    std::vector<Location*> mTopo;

    llvm::DenseMap<Location*, size_t> mLocNumbers;
    llvm::DenseMap<CallTransition*, CallInfo> mCalls;
    llvm::DenseSet<CallTransition*> mOpenCalls;
};

}

using namespace gazer;

std::unique_ptr<SafetyResult> BoundedModelChecker::check()
{
    // We are using a dynamic programming-based approach.
    // As the CFG is a DAG after unrolling, we can create a topoligcal sort
    // of its blocks. Then we create an array with the size of numBB, and
    // perform DP as the following:
    //  (0) dp[0] := True (as the entry node is always reachable)
    //  (1) dp[i] := Or(forall p in pred(i): And(dp[p], SMT(p,i)))
    // This way dp[err] will contain the SMT encoding of all bounded error paths.

    // The entry node is always reachable.
    mVC[0].expr = mExprBuilder.True();

    // Calculate the initial verification condition
    this->updateVC(/*startIdx=*/ 1);

    unsigned tmp = 0;
    // Let's do some verification
    for (size_t bound = 0; bound < MaxBound; ++bound) {
        llvm::outs() << "Iteration " << bound << "\n";

        while (true) {
            llvm::outs() << "  Under-approximating.\n";

            size_t errIdx = mLocNumbers[mError];
            ExprPtr formula = mVC[errIdx].expr;

            auto solver = mSolverFactory.createSolver(mSystem.getContext());
            llvm::outs() << "    Transforming formula...\n";
            solver->add(formula);

            llvm::outs() << "    Running solver...\n";
            auto status = solver->run();

            if (status == Solver::SAT) {
                llvm::outs() << "  Under-approximated formula is SAT.\n";
                return SafetyResult::CreateFail(0);
            }

            // Now try to over-approximate.
            llvm::outs() << "  Over-approximating.\n";

            size_t first = mVC.size() - 1;

            mOpenCalls.clear();
            for (auto& callPair : mCalls) {
                CallTransition* call = callPair.first;
                CallInfo& info = callPair.second;

                if (info.cost >= bound) {
                    info.overApprox = mExprBuilder.False();
                    continue;
                }

                size_t targetIdx = mLocNumbers[call->getSource()];
                if (targetIdx < first) {
                    first = targetIdx;
                }
                info.overApprox = mExprBuilder.True();
                mOpenCalls.insert(call);
            }

            this->updateVC(first);
            formula = mVC[errIdx].expr;

            solver->reset();
            solver->add(formula);

            status = solver->run();
            if (status == Solver::SAT) {
                llvm::outs() << "      Over-approximated formula is SAT.\n";
                llvm::outs() << "      Checking counterexample....\n";
                // We have a counterexample, but it may be spurious.

                llvm::outs() << "  Inlining calls...\n";

                for (CallTransition* call : mOpenCalls) {
                    llvm::DenseMap<Variable*, Variable*> vmap;
                    inlineCallIntoRoot(call, vmap, "_call" + llvm::Twine(tmp++));
                }

            } else if (status == Solver::UNSAT) {
            }
        }

    }
}

void BoundedModelChecker::updateVC(size_t startIdx)
{
    for (size_t i = startIdx; i < mVC.size(); ++i) {
        Location* loc = mTopo[i];
        ExprVector exprs;

        for (Transition* edge : loc->incoming()) {
            auto predIt = mLocNumbers.find(edge->getSource());
            assert(predIt != mLocNumbers.end()
                && "All locations must be present in the location map");

            size_t predIdx = predIt->second;
            assert(predIdx < i
                && "Predecessors must be before block in a topological sort. "
                "Maybe there is a loop in the automaton?");

            ExprPtr formula = mVC[predIdx].expr;

            if (auto assignEdge = llvm::dyn_cast<AssignTransition>(edge)) {
                ExprVector assigns;
                std::transform(assignEdge->begin(), assignEdge->end(), std::back_inserter(assigns), [this](const VariableAssignment& varAssign) {
                    return this->mExprBuilder.Eq(varAssign.getVariable()->getRefExpr(), varAssign.getValue());
                });

                formula = mExprBuilder.Eq(formula, mExprBuilder.And(assigns));
            } else if (auto callEdge = llvm::dyn_cast<CallTransition>(edge)) {
                formula = mExprBuilder.And(formula, mCalls[callEdge].overApprox);
            }

            exprs.push_back(formula);
        }

        if (!exprs.empty()) {
            mVC[i].expr = mExprBuilder.Or(exprs);
        }
    }
}

void BoundedModelChecker::inlineCallIntoRoot(
    CallTransition* call,
    llvm::DenseMap<Variable*, Variable*>& vmap,
    llvm::Twine suffix
) {
    CallInfo& info = mCalls[call];
    auto callee = call->getCalledAutomaton();

    llvm::DenseMap<Location*, Location*> locToLocMap;
    llvm::DenseMap<Transition*, Transition*> edgeToEdgeMap;

    ExprRewrite rewrite;

    // Clone all local variables into the parent
    for (Variable& local : callee->locals()) {
        auto varname = local.getName() + suffix;
        auto newLocal = mRoot->createLocal(varname.str(), local.getType());
        vmap[&local] = newLocal;
        rewrite[&local] = newLocal->getRefExpr();
    }

    for (size_t i = 0; i < callee->getNumInputs(); ++i) {
        Variable* input = callee->getInput(i);

        auto varname = input->getName() + suffix;
        vmap[input] = mRoot->createInput(varname.str(), input->getType());
        rewrite[input] = call->getInputArgument(i);
    }

    // Insert the locations
    for (auto& origLoc : callee->nodes()) {
        auto newLoc = mRoot->createLocation();
        locToLocMap[origLoc.get()] = newLoc;

        if (origLoc->isError()) {
            mRoot->createAssignTransition(newLoc, mError, mExprBuilder.True());
        }
    }

    // Transform the edges
    for (auto& origEdge : callee->edges()) {
        Transition* newEdge = nullptr;
        Location* source = locToLocMap[origEdge->getSource()];
        Location* target = locToLocMap[origEdge->getTarget()];

        if (auto assign = llvm::dyn_cast<AssignTransition>(origEdge.get())) {
            // Transform the assignments of this edge to use the new variables.
            std::vector<VariableAssignment> newAssigns;
            std::transform(
                assign->begin(), assign->end(), std::back_inserter(newAssigns),
                [&vmap, &rewrite] (const VariableAssignment& origAssign) {
                    return VariableAssignment {
                        vmap[origAssign.getVariable()],
                        rewrite.visit(origAssign.getValue())
                    };
                }
            );

            newEdge = mRoot->createAssignTransition(
                source, target, rewrite.visit(assign->getGuard()), newAssigns
            );
        } else if (auto nestedCall = llvm::dyn_cast<CallTransition>(origEdge.get())) {
            ExprVector newArgs;
            std::vector<VariableAssignment> newOuts;

            std::transform(
                nestedCall->input_begin(), nestedCall->input_end(),
                std::back_inserter(newArgs),
                [&rewrite](const ExprPtr& expr) { return rewrite.visit(expr); }
            );
            std::transform(
                nestedCall->output_begin(), nestedCall->output_end(),
                std::back_inserter(newOuts),
                [&rewrite, &vmap](const VariableAssignment& origAssign) {
                    return VariableAssignment{
                        vmap[origAssign.getVariable()],
                        rewrite.visit(origAssign.getValue())
                    };
                }
            );

            auto callEdge = mRoot->createCallTransition(
                source, target,
                rewrite.visit(call->getGuard()),
                nestedCall->getCalledAutomaton(),
                newArgs, newOuts
            );

            newEdge = callEdge;
            mCalls[callEdge].cost = mCalls[call].cost + 1;
            mCalls[callEdge].overApprox = mExprBuilder.False();
        } else {
            llvm_unreachable("Unknown transition kind!");
        }

        edgeToEdgeMap[origEdge.get()] = newEdge;
    }

    Location* before = call->getSource();
    Location* after  = call->getTarget();

    mRoot->removeEdge(call);
    auto newEdge = mRoot->createAssignTransition(
        before, locToLocMap[callee->getEntry()]
    );

    // Do the output assignments.
    std::vector<VariableAssignment> outputAssigns;
    // TODO

    mRoot->createAssignTransition(
        locToLocMap[callee->getExit()], after
    );
}