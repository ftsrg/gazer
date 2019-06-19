#include "gazer/Verifier/BoundedModelChecker.h"

#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/Solver/Solver.h"
#include "gazer/Automaton/Cfa.h"
#include "gazer/Trace/SafetyResult.h"
#include "gazer/Core/Expr/ExprRewrite.h"

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/Support/Debug.h>

#include <deque>

#define DEBUG_TYPE "BoundedModelChecker"

namespace gazer
{

llvm::cl::opt<unsigned> MaxBound("bound", llvm::cl::desc("Maximum iterations for the bounded model checker."));
llvm::cl::opt<unsigned> EagerUnroll("eager-unroll", llvm::cl::desc("Eager unrolling bound."), llvm::cl::init(0));

llvm::cl::opt<bool> VerifierDebug("debug-verif", llvm::cl::desc("Print verifier debug info"));
llvm::cl::opt<bool> ViewCfa("view-cfa", llvm::cl::desc("View the generated CFA."));
llvm::cl::opt<bool> DumpCfa("debug-dump-cfa", llvm::cl::desc("Dump the generated CFA after each inlining step."));

class BoundedModelCheckerImpl
{
    struct VcCell
    {
        ExprPtr expr;
        unsigned numCalls = 0;
        unsigned callCost = 0;

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
    BoundedModelCheckerImpl(
        AutomataSystem& system,
        ExprBuilder& builder,
        SolverFactory& solverFactory
    );

    std::unique_ptr<SafetyResult> check();

private:
    void updateVC(size_t startIdx);

    void updateBackwardConditions();

    void inlineCallIntoRoot(
        CallTransition* call,
        llvm::DenseMap<Variable*, Variable*>& vmap,
        llvm::Twine suffix
    );
    void clearInfeasiblePaths();

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
    std::unordered_map<Cfa*, std::vector<Location*>> mTopoSortMap;
};

}

using namespace gazer;

std::unique_ptr<SafetyResult> BoundedModelChecker::check(AutomataSystem& system)
{
    auto builder = CreateFoldingExprBuilder(system.getContext());
    BoundedModelCheckerImpl impl{system, *builder, mSolverFactory};
    return impl.check();
}

BoundedModelCheckerImpl::BoundedModelCheckerImpl(
    AutomataSystem& system, ExprBuilder& builder, SolverFactory& solverFactory
) : mSystem(system), mExprBuilder(builder), mSolverFactory(solverFactory)
    {
        // TODO: Make this more flexible
        mRoot = mSystem.getAutomatonByName("main");

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
            mRoot->createAssignTransition(mRoot->getEntry(), mError, mExprBuilder.False());
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
        }

        // Create the topological sorts of the reversed graphs.
        // This will cause the error location to be at the front for most input programs.
        for (Cfa& cfa : mSystem) {
            auto poBegin = llvm::po_begin(cfa.getEntry());
            auto poEnd = llvm::po_end(cfa.getEntry());

            auto& topoVec = mTopoSortMap[&cfa];
            topoVec.insert(mTopo.end(), poBegin, poEnd);
            std::reverse(topoVec.begin(), topoVec.end());
        }

        auto& mainTopo = mTopoSortMap[mRoot];
        mTopo.insert(mTopo.end(), mainTopo.begin(), mainTopo.end());

        for (size_t i = 0; i < mTopo.size(); ++i) {
            mLocNumbers[mTopo[i]] = i;
        }

        // Fill the initial VC vector with False
        mVC.resize(mTopo.size());
        std::fill(mVC.begin(), mVC.end(), VcCell{mExprBuilder.False()});

        // Insert initial call approximations.
        for (auto& edge : mRoot->edges()) {
            if (auto call = llvm::dyn_cast<CallTransition>(edge.get())) {
                mCalls[call].overApprox = mExprBuilder.False();
                mCalls[call].cost = 1;
            }
        }
    }

std::unique_ptr<SafetyResult> BoundedModelCheckerImpl::check()
{
    if (ViewCfa) {
        for (Cfa& cfa : mSystem) {
            cfa.view();
        }
    }

    // We are using a dynamic programming-based approach.
    // As the CFA is required to be a DAG, we have a topoligcal sort
    // of its locations. Then we create an array with the size of numLocs, and
    // perform DP as the following:
    //  (0) dp[0] := True (as the entry node is always reachable)
    //  (1) dp[i] := Or(forall p in pred(i): And(dp[p], SMT(p,i)))
    // This way dp[err] will contain the SMT encoding of all bounded error paths.

    if (EagerUnroll > MaxBound) {
        llvm::errs() << "ERROR: Eager unrolling bound is larger than maximum bound.\n";
        return SafetyResult::CreateUnknown();
    }

    unsigned tmp = 0;
    for (size_t bound = 1; bound <= EagerUnroll; ++bound) {
        mOpenCalls.clear();
        for (auto& entry : mCalls) {
            CallTransition* call = entry.first;
            CallInfo& info = entry.second;

            if (info.cost <= bound) {
                mOpenCalls.insert(call);
            }
        }

        for (CallTransition* call : mOpenCalls) {
            llvm::DenseMap<Variable*, Variable*> vmap;
            inlineCallIntoRoot(call, vmap, "_call" + llvm::Twine(tmp++));
            mCalls.erase(call);
        }
    }    

    // The entry is always reachable from itself.
    mVC[0].expr = mExprBuilder.True();

    // Calculate the initial verification condition
    this->updateVC(1);
    auto solver = mSolverFactory.createSolver(mSystem.getContext());

    // Let's do some verification.
    for (size_t bound = EagerUnroll + 1; bound <= MaxBound; ++bound) {
        llvm::outs() << "Iteration " << bound << "\n";

        while (true) {
            unsigned numUnhandledCallSites = 0;
            llvm::outs() << "  Under-approximating.\n";

            size_t errIdx = mLocNumbers[mError];
            ExprPtr formula = mVC[errIdx].expr;

            solver->reset();
            llvm::outs() << "    Transforming formula...\n";
            solver->add(formula);

            llvm::outs() << "    Running solver...\n";
            auto status = solver->run();

            if (status == Solver::SAT) {
                llvm::outs() << "  Under-approximated formula is SAT.\n";
                //LLVM_DEBUG(formula->print(llvm::dbgs()));
                solver->getModel().print(llvm::outs());
                return SafetyResult::CreateFail(0);
            }

            // Now try to over-approximate.
            llvm::outs() << "  Over-approximating.\n";

            size_t first = mVC.size() - 1;

            mOpenCalls.clear();
            for (auto& callPair : mCalls) {
                CallTransition* call = callPair.first;
                CallInfo& info = callPair.second;

                if (info.cost > bound) {
                    LLVM_DEBUG(
                        llvm::dbgs() << "  Skipping " << *call
                        << ": inline cost is greater than bound (" << info.cost << " > " << bound << ").\n"
                    );
                    info.overApprox = mExprBuilder.False();
                    ++numUnhandledCallSites;
                    continue;
                }

                size_t targetIdx = mLocNumbers[call->getSource()];
                if (targetIdx < first) {
                    first = targetIdx;
                }
                info.overApprox = mExprBuilder.True();
                mOpenCalls.insert(call);
            }

            this->updateVC(1);
            formula = mVC[errIdx].expr;

            solver->reset();
            solver->add(formula);

            llvm::outs() << "    Running solver...\n";
            status = solver->run();
            if (status == Solver::SAT) {
                llvm::outs() << "      Over-approximated formula is SAT.\n";
                llvm::outs() << "      Checking counterexample....\n";
                // We have a counterexample, but it may be spurious.

                llvm::outs() << "    Inlining calls...\n";

                for (CallTransition* call : mOpenCalls) {
                    llvm::DenseMap<Variable*, Variable*> vmap;
                    inlineCallIntoRoot(call, vmap, "_call" + llvm::Twine(tmp++));
                    mCalls.erase(call);
                }
                mRoot->clearDisconnectedElements();

                if (DumpCfa) {
                    mRoot->view();
                }

                this->updateVC(1);
            } else if (status == Solver::UNSAT) {
                llvm::outs() << "  Over-approximated formula is UNSAT.\n";
                if (numUnhandledCallSites == 0) {
                    // If we have no unhandled call sites,
                    // the program is guaranteed to be safe at this point.
                    return SafetyResult::CreateSuccess();
                }  else if (bound == MaxBound) {
                    // The maximum bound was reached.
                    return SafetyResult::CreateSuccess();
                } else {
                    // Try with an increased bound.
                    llvm::outs() << "    Open call sites still present. Increasing bound.\n";
                    break;
                }
            }
        }
    }

    return SafetyResult::CreateSuccess();
}

void BoundedModelCheckerImpl::updateBackwardConditions()
{
    for (size_t i = 0; i < mVC.size(); ++i) {
        Location* loc = mTopo[i];
        ExprVector exprs;

        unsigned numCalls = 0;
        unsigned callCost = 0;

        for (Transition* edge : loc->outgoing()) {
            auto predIt = mLocNumbers.find(edge->getTarget());
            assert(predIt != mLocNumbers.end()
                && "All locations must be present in the location map");

            size_t predIdx = predIt->second;
            assert(predIdx < i
                && "Successors must be before block in a inverse topological sort. "
                "Maybe there is a loop in the automaton?");

            numCalls += mVC[predIdx].numCalls;
            callCost += mVC[predIdx].callCost;

            ExprPtr formula = mExprBuilder.And(mVC[predIdx].expr, edge->getGuard());

            if (auto assignEdge = llvm::dyn_cast<AssignTransition>(edge)) {
                ExprVector assigns;
                std::transform(assignEdge->begin(), assignEdge->end(), std::back_inserter(assigns), [this](const VariableAssignment& varAssign) {
                    return this->mExprBuilder.Eq(varAssign.getVariable()->getRefExpr(), varAssign.getValue());
                });

                formula = mExprBuilder.And(formula, mExprBuilder.And(assigns));
            } else if (auto callEdge = llvm::dyn_cast<CallTransition>(edge)) {
                ++numCalls;
                callCost += mCalls[callEdge].cost;
                LLVM_DEBUG(llvm::dbgs() << "  Over-approximation for edge " << *callEdge << ": " << *mCalls[callEdge].overApprox << "\n");
                formula = mExprBuilder.And(formula, mCalls[callEdge].overApprox);
            }

            exprs.push_back(formula);
        }

        mVC[i].expr = exprs.empty() ? mExprBuilder.False() : mExprBuilder.Or(exprs);
        mVC[i].numCalls = numCalls;
        mVC[i].callCost = callCost;
    }
}

void BoundedModelCheckerImpl::updateVC(size_t startIdx)
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

            ExprPtr formula = mExprBuilder.And(mVC[predIdx].expr, edge->getGuard());

            if (auto assignEdge = llvm::dyn_cast<AssignTransition>(edge)) {
                ExprVector assigns;
                std::transform(assignEdge->begin(), assignEdge->end(), std::back_inserter(assigns), [this](const VariableAssignment& varAssign) {
                    return this->mExprBuilder.Eq(varAssign.getVariable()->getRefExpr(), varAssign.getValue());
                });

                if (!assigns.empty()) {
                    formula = mExprBuilder.And(formula, mExprBuilder.And(assigns));
                }
            } else if (auto callEdge = llvm::dyn_cast<CallTransition>(edge)) {
                LLVM_DEBUG(llvm::dbgs() << "  Over-approximation for edge " << *callEdge << ": " << *mCalls[callEdge].overApprox << "\n");
                formula = mExprBuilder.And(formula, mCalls[callEdge].overApprox);
            }

            exprs.push_back(formula);
        }

        if (!exprs.empty()) {
            mVC[i].expr = mExprBuilder.Or(exprs);
        }
    }
}

void BoundedModelCheckerImpl::inlineCallIntoRoot(
    CallTransition* call,
    llvm::DenseMap<Variable*, Variable*>& vmap,
    llvm::Twine suffix
) {
    //LLVM_DEBUG(
        llvm::errs() << " Inlining call " << *call
            << " edge " << call->getSource()->getId()
            << " --> " << call->getTarget()->getId()
            << "\n";
    //);

    CallInfo& info = mCalls[call];
    auto callee = call->getCalledAutomaton();

    llvm::DenseMap<Location*, Location*> locToLocMap;
    llvm::DenseMap<Transition*, Transition*> edgeToEdgeMap;

    ExprRewrite rewrite(mExprBuilder);

    // Clone all local variables into the parent
    for (Variable& local : callee->locals()) {
        if (!callee->isOutput(&local)) {
            auto varname = (local.getName() + suffix).str();
            auto newLocal = mRoot->createLocal(varname, local.getType());
            vmap[&local] = newLocal;
            rewrite[&local] = newLocal->getRefExpr();
        }
    }

    for (size_t i = 0; i < callee->getNumInputs(); ++i) {
            Variable* input = callee->getInput(i);
        if (!callee->isOutput(input)) {

            auto varname = input->getName() + suffix;
            vmap[input] = mRoot->createInput(varname.str(), input->getType());
            rewrite[input] = call->getInputArgument(i);
        }
    }

    for (size_t i = 0; i < callee->getNumOutputs(); ++i) {
        Variable* output = callee->getOutput(i);
        vmap[output] = call->getOutputArgument(i).getVariable();
        rewrite[output] = call->getOutputArgument(i).getVariable()->getRefExpr();
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
    auto addr = [](auto& ptr) { return ptr.get(); };

    std::vector<Transition*> edges(
        llvm::map_iterator(callee->edge_begin(), addr),
        llvm::map_iterator(callee->edge_end(), addr)
    );

    for (auto origEdge : edges) {
        Transition* newEdge = nullptr;
        Location* source = locToLocMap[origEdge->getSource()];
        Location* target = locToLocMap[origEdge->getTarget()];

        if (auto assign = llvm::dyn_cast<AssignTransition>(origEdge)) {
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
        } else if (auto nestedCall = llvm::dyn_cast<CallTransition>(origEdge)) {
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
                rewrite.visit(nestedCall->getGuard()),
                nestedCall->getCalledAutomaton(),
                newArgs, newOuts
            );

            newEdge = callEdge;
            mCalls[callEdge].cost = info.cost + 1;
            mCalls[callEdge].overApprox = mExprBuilder.False();
        } else {
            llvm_unreachable("Unknown transition kind!");
        }

        edgeToEdgeMap[origEdge] = newEdge;
    }

    Location* before = call->getSource();
    Location* after  = call->getTarget();

    mRoot->createAssignTransition(
        before, locToLocMap[callee->getEntry()], call->getGuard()
    );

    // Do the output assignments.
    std::vector<VariableAssignment> outputAssigns;

    // for (size_t i = 0; i < call->getNumOutputs(); ++i) {
    //     VariableAssignment output = call->getOutputArgument(i);
    //     LLVM_DEBUG(llvm::dbgs() << "Transforming output assignment " << i << ": " << output << "\n");
    //     outputAssigns.emplace_back(output.getVariable(), rewrite.visit(output.getValue()));
    // }

    mRoot->createAssignTransition(
        locToLocMap[callee->getExit()], after /*, mExprBuilder.True(), outputAssigns */
    );

    // Add the new locations to the topological sort.
    // As every inlined location should come between the source and target of the original call transition,
    // we will insert them there in the topo sort.
    auto& oldTopo = mTopoSortMap[callee];
    auto getInlinedLocation = [&locToLocMap](Location* loc) {
        return locToLocMap[loc];
    };    

    size_t callIdx = mLocNumbers[call->getTarget()];
    auto callPos = std::next(mTopo.begin(), callIdx);
    auto insertPos = mTopo.insert(callPos,
        llvm::map_iterator(oldTopo.begin(), getInlinedLocation),
        llvm::map_iterator(oldTopo.end(), getInlinedLocation)
    );

    mVC.resize(mTopo.size());

    // Update the location numbers
    for (auto it = insertPos, ie = mTopo.end(); it != ie; ++it) {
        size_t idx = std::distance(mTopo.begin(), it);
        mLocNumbers[*it] = idx;
        mVC[idx].expr = mExprBuilder.False();
    }

    mRoot->disconnectEdge(call);
}

void BoundedModelCheckerImpl::clearInfeasiblePaths()
{
    for (auto& edge : mRoot->edges()) {
    }
}
