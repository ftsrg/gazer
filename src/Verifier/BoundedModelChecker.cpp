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
#include "BoundedModelCheckerImpl.h"

#include "gazer/Core/Expr/ExprRewrite.h"
#include "gazer/Core/Expr/ExprUtils.h"
#include "gazer/Automaton/CfaUtils.h"
#include "gazer/Automaton/CfaTransforms.h"

#include "gazer/Support/Stopwatch.h"
#include "gazer/Support/Warnings.h"

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/DepthFirstIterator.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Debug.h>

#include <sstream>

#define DEBUG_TYPE "BoundedModelChecker"

using namespace gazer;

namespace
{
    const llvm::cl::opt<bool> NoDomPush("bmc-no-dom-push", llvm::cl::Hidden);
    const llvm::cl::opt<bool> NoPostDomPush("bmc-no-postdom-push", llvm::cl::Hidden);
} // end anonymous namespace

auto BoundedModelChecker::check(AutomataSystem& system, CfaTraceBuilder& traceBuilder)
    -> std::unique_ptr<VerificationResult>
{
    std::unique_ptr<ExprBuilder> builder;

    if (mSettings.simplifyExpr) {
        builder = CreateFoldingExprBuilder(system.getContext());
    } else {
        builder = CreateExprBuilder(system.getContext());
    }
    BoundedModelCheckerImpl impl{system, *builder, mSolverFactory, traceBuilder, mSettings};

    auto result = impl.check();

    impl.printStats(llvm::outs());

    return result;
}

BoundedModelCheckerImpl::BoundedModelCheckerImpl(
    AutomataSystem& system,
    ExprBuilder& builder,
    SolverFactory& solverFactory,
    CfaTraceBuilder& traceBuilder,
    BmcSettings settings
) : mSystem(system),
    mExprBuilder(builder),
    mSolver(solverFactory.createSolver(system.getContext())),
    mTraceBuilder(traceBuilder),
    mSettings(settings)
{
    Cfa* main = mSystem.getMainAutomaton();
    assert(main != nullptr && "The main automaton must exist!");

    CfaCloneResult cloneResult = CloneCfa(*main);
    mRoot = cloneResult.clonedCfa;
    for (auto& [oldLoc, newLoc] : cloneResult.locToLocMap) {
        mInlinedLocations[newLoc] = oldLoc;
    }
    for (auto& [oldVar, newVar] : cloneResult.varToVarMap) {
        mInlinedVariables[newVar] = oldVar;
    }
}

void BoundedModelCheckerImpl::createTopologicalSorts()
{
    for (Cfa& cfa : mSystem) {
        mTopoSortMap.try_emplace(&cfa, cfa);
    }

    mTopo = CfaTopoSort(*mRoot);
}

auto BoundedModelCheckerImpl::initializeErrorField() -> bool
{
    // Set the verification goal - a single error location.
    llvm::SmallVector<Location*, 1> errors;
    Type* errorFieldType = nullptr;
    for (const auto& [location, errExpr] : mRoot->errors()) {
        errors.push_back(location);
        errorFieldType = &errExpr->getType();
    }

    // Try to find the error field type from using another CFA.
    if (errorFieldType == nullptr) {
        for (const Cfa& cfa : mSystem) {
            if (cfa.error_begin() != cfa.error_end()) {
                auto errExpr = cfa.error_begin()->second;
                errorFieldType = &errExpr->getType();
            }
        }
    }

    // If the error field type is still not known, there are no errors present in the system,
    // the program is safe by definition
    if (errorFieldType == nullptr) {
        return false;
    }

    mError = mRoot->createErrorLocation();
    mErrorFieldVariable = mRoot->createLocal("__error_field", *errorFieldType);

    if (errors.empty()) {
        // If there are no error locations in the main automaton, they might still exist in a called CFA.
        // A dummy error location will be used as a goal.
        mRoot->createAssignTransition(mRoot->getEntry(), mError, mExprBuilder.False(), {
            VariableAssignment{ mErrorFieldVariable, mExprBuilder.BvLit(0, 16) }
        });
    } else {
        // The error location which will be directly reachable from already existing error locations.
        for (Location* err : errors) {
            mRoot->createAssignTransition(err, mError, mExprBuilder.True(), {
                VariableAssignment { mErrorFieldVariable, mRoot->getErrorFieldExpr(err) }
            });
        }
    }

    // Create a dummy transition from the error location into the exit location.
    mRoot->createAssignTransition(mError, mRoot->getExit(), mExprBuilder.False());

    return true;
}

void BoundedModelCheckerImpl::initializeCallApproximations()
{
    for (Transition* edge : mRoot->edges()) {
        if (auto* call = llvm::dyn_cast<CallTransition>(edge)) {
            mCalls[call].overApprox = mExprBuilder.False();
            mCalls[call].callChain.push_back(call->getCalledAutomaton());
        }
    }
}

void BoundedModelCheckerImpl::removeIrrelevantLocations()
{
    // The CFA construction algorithm must guarantee that the automata are
    // connected graphs where every location is reachable from the entry,
    // and all locations have a path (possibly annotated with an 'assume false')
    // to the exit location.
    llvm::df_iterator_default_set<Location*> visited;
    auto begin = llvm::idf_ext_begin(mError, visited);
    auto end = llvm::idf_ext_end(mError, visited);

    // Do the DFS visit
    while (begin != end) {
        ++begin;
    }

    // Disconnect all unvisited locations, minus the exit location.
    visited.insert(mRoot->getExit());
    for (Location* loc : mRoot->nodes()) {
        if (visited.count(loc) == 0) {
            mRoot->disconnectNode(loc);
        }
    }

    mRoot->clearDisconnectedElements();
}

void BoundedModelCheckerImpl::performEagerUnrolling()
{
    for (size_t bound = 0; bound < mSettings.eagerUnroll; ++bound) {
        llvm::outs() << "Eager iteration " << bound << "\n";
        mOpenCalls.clear();
        for (auto& [call, info] : mCalls) {
            if (info.getCost() <= bound) {
                mOpenCalls.insert(call);
            }
        }

        llvm::SmallVector<CallTransition*, 16> callsToInline;
        for (CallTransition* call : mOpenCalls) {
            inlineCallIntoRoot(call, "_call" + llvm::Twine(mTmp++), callsToInline);
            mCalls.erase(call);
        }
    }
}

auto BoundedModelCheckerImpl::check() -> std::unique_ptr<VerificationResult>
{
    // Initialize error field
    bool hasErrorLocation = this->initializeErrorField();
    if (!hasErrorLocation) {
        llvm::outs() << "No error location is present or it was discarded by the frontend.\n";
        return VerificationResult::CreateSuccess();
    }

    // Do some basic pre-processing step: remove all locations from which the error
    // location is not reachable.
    this->removeIrrelevantLocations();

    // Create the topological sorts
    this->createTopologicalSorts();

    // Insert initial call approximations.
    this->initializeCallApproximations();

    // See if -debug-dump-cfa is enabled.
    this->dumpAutomataIfRequested();

    // Initialize the path condition calculator
    mPathConditions = std::make_unique<PathConditionCalculator>(
        mTopo,
        mExprBuilder,
        [this](CallTransition* call) {
          return mCalls[call].overApprox;
        },
        [this](Location* l, const ExprPtr& e) {
          mPredecessors.insert(l, e);
        }
    );

    // Do eager unrolling, if requested
    if (mSettings.eagerUnroll >= mSettings.maxBound) {
        emit_error("maximum bound (%d) must be larger than eager unrolling bound (%d)", mSettings.maxBound, mSettings.eagerUnroll);
        return VerificationResult::CreateUnknown();
    }

    this->performEagerUnrolling();

    mStats.NumBeginLocs = mRoot->getNumLocations();
    mStats.NumBeginLocals = mRoot->getNumLocals();

    mTopLoc = mRoot->getEntry();
    mBottomLoc = mError;
    mSkipUnderApprox = false;

    // Let's do some verification.
    for (unsigned bound = mSettings.eagerUnroll + 1; bound <= mSettings.maxBound; ++bound) {
        llvm::outs() << "Iteration " << bound << "\n";

        while (true) {
            unsigned numUnhandledCallSites = 0;
            ExprPtr formula;
            Solver::SolverStatus status;

            // Start with an under-approximation step: remove all calls from the automaton
            // (by replacing them with 'assume false') and check if the error location is
            // reachable. If it is reachable, we have found a true counterexample.
            status = this->underApproximationStep();
            if (status == Solver::SAT) {
                llvm::outs() << "  Under-approximated formula is SAT.\n";
                return this->createFailResult();
            }
            mSkipUnderApprox = false;

            // If the under-approximated formula was UNSAT, there is no feasible path from start
            // to the error location which does not involve a call. Find the lowest common dominator
            // of all calls, and set is as the start location. Similarly, we can calculate the
            // highest common post-dominator for the error location of all calls to update the
            // target state. These nodes are the lowest common ancestors (LCA) of the calls in
            // the (post-)dominator trees.
            auto lca = this->findCommonCallAncestor(mTopLoc, mBottomLoc);

            this->push();
            if (lca.first != nullptr) {
                LLVM_DEBUG(llvm::dbgs() << "Found LCA, " << lca.first->getId() << ".\n");
                assert(lca.second != nullptr);

                mSolver->add(mPathConditions->encode(mTopLoc, lca.first));
                mSolver->add(mPathConditions->encode(lca.second, mBottomLoc));

                // Run the solver and check whether top and bottom are consistent -- if not,
                // we can return that the program is safe as all possible error paths will
                // encode these program parts.
                status = this->runSolver();

                if (status == Solver::UNSAT) {
                    llvm::outs() << "    Start and target points are inconsistent, no errors are reachable.\n";
                    return VerificationResult::CreateSuccess();
                }
            } else {
                LLVM_DEBUG(llvm::dbgs() << "No calls present, LCA is " << mTopLoc->getId() << ".\n");
                lca = { mTopLoc, mBottomLoc };
            }

            // Now try to over-approximate.
            llvm::outs() << "  Over-approximating.\n";

            // Find all calls which are to be over-approximated: that is, calls that have a cost
            // less than the current bound
            mOpenCalls.clear();
            numUnhandledCallSites += collectOpenCalls(bound);

            this->push();

            llvm::outs() << "    Calculating verification condition...\n";
            formula = mPathConditions->encode(lca.first, lca.second);
            this->dumpFormulaIfRequested(formula);

            llvm::outs() << "    Transforming formula...\n";
            mSolver->add(formula);
            this->dumpSolverIfRequested();

            status = this->runSolver();

            if (status == Solver::SAT) {
                llvm::outs() << "      Over-approximated formula is SAT.\n";
                llvm::outs() << "      Checking counterexample...\n";

                // We have a counterexample, but it may be spurious.
                auto model = mSolver->getModel();
                this->inlineOpenCalls(*model, bound);

                this->pop();
                mTopLoc = lca.first;
                mBottomLoc = lca.second;
            } else if (status == Solver::UNSAT) {
                llvm::outs() << "  Over-approximated formula is UNSAT.\n";

                if (auto boundResult = this->checkBound(numUnhandledCallSites, bound)) {
                    return boundResult;
                }

                // Try with an increased bound.
                llvm::outs() << "    Open call sites still present. Increasing bound.\n";
                this->pop();
                mTopLoc = lca.first;
                mBottomLoc = lca.second;

                // Skip redundant under-approximation step - all calls in the system are
                // under-approximated with 'False', which will not change when we jump
                // back to the under-approximation step.
                mSkipUnderApprox = true;
                break;
            } else {
                llvm_unreachable("Unknown solver status.");
            }
        }
    }

    return VerificationResult::CreateBoundReached();
}

unsigned BoundedModelCheckerImpl::collectOpenCalls(size_t bound)
{
    unsigned unhandledCalls = 0;

    for (auto& [call, info] : mCalls) {
        if (info.getCost() > bound) {
            LLVM_DEBUG(
                llvm::dbgs() << "  Skipping " << *call
                << ": inline cost is greater than bound (" <<
                info.getCost() << " > " << bound << ").\n"
            );
            info.overApprox = mExprBuilder.False();
            ++unhandledCalls;
            continue;
        }

        info.overApprox = mExprBuilder.True();
        mOpenCalls.insert(call);
    }

    return unhandledCalls;
}

auto BoundedModelCheckerImpl::underApproximationStep() -> Solver::SolverStatus
{
    if (mSkipUnderApprox) {
        return Solver::UNKNOWN;
    }

    llvm::outs() << "  Under-approximating.\n";

    for (auto& [_, callInfo] : mCalls) {
        callInfo.overApprox = mExprBuilder.False();
    }

    ExprPtr formula = mPathConditions->encode(mTopLoc, mBottomLoc);

    this->push();
    llvm::outs() << "    Transforming formula...\n";
    if (mSettings.dumpFormula) {
        formula->print(llvm::errs());
    }

    mSolver->add(formula);

    if (mSettings.dumpSolver) {
        mSolver->dump(llvm::errs());
    }

    auto status = this->runSolver();

    this->pop();
    return status;
}

void BoundedModelCheckerImpl::inlineOpenCalls(Model& model, size_t bound)
{
    llvm::SmallVector<CallTransition*, 16> callsToInline;
    this->findOpenCallsInCex(model, callsToInline);

    llvm::outs() << "    Inlining calls...\n";
    while (!callsToInline.empty()) {
        CallTransition* call = callsToInline.pop_back_val();
        llvm::outs() << "      Inlining " << call->getSource()->getId() << " --> "
            << call->getTarget()->getId() << " "
            << call->getCalledAutomaton()->getName() << "\n";
        mStats.NumInlined++;

        llvm::SmallVector<CallTransition*, 4> newCalls;
        this->inlineCallIntoRoot(
            call, "_call" + llvm::Twine(mTmp++), newCalls
        );
        mCalls.erase(call);
        mOpenCalls.erase(call);

        for (CallTransition* newCall : newCalls) {
            if (mCalls[newCall].getCost() <= bound) {
                callsToInline.push_back(newCall);
            }
        }
    }

    mRoot->clearDisconnectedElements();

    mStats.NumEndLocs = mRoot->getNumLocations();
    mStats.NumEndLocals = mRoot->getNumLocals();
    if (mSettings.debugDumpCfa) {
        mRoot->view();
    }
}

auto BoundedModelCheckerImpl::findCommonCallAncestor(Location* fwd, Location* bwd)
    -> std::pair<Location*, Location*>
{
    // Calculate the closest common (post-)dominator for all call nodes
    std::vector<Transition*> targets;
    std::transform(mCalls.begin(), mCalls.end(), std::back_inserter(targets), [](auto& pair) {
        return pair.first;
    });

    Location* dom;
    Location* pdom;

    if (!NoDomPush) {
        dom = findLowestCommonDominator(targets, mTopo, fwd);
    } else {
        dom = fwd;
    }

    if (!NoPostDomPush) {
        pdom = findHighestCommonPostDominator(targets, mTopo, bwd);
    } else {
        pdom = bwd;
    }

    return { dom, pdom };
}

void BoundedModelCheckerImpl::findOpenCallsInCex(Model& model, llvm::SmallVectorImpl<CallTransition*>& callsInCex)
{
    auto cex = bmc::BmcCex{mError, *mRoot, model, mPredecessors};

    for (auto state : cex) {
        auto* call = llvm::dyn_cast_or_null<CallTransition>(state.getOutgoingTransition());
        if (call != nullptr && mOpenCalls.count(call) != 0) {
            callsInCex.push_back(call);
            if (callsInCex.size() == mOpenCalls.size()) {
                // All possible calls were encountered, no point in iterating further.
                break;
            }
        }
    }
}

void BoundedModelCheckerImpl::inlineCallIntoRoot(
    CallTransition* call,
    const llvm::Twine& suffix,
    llvm::SmallVectorImpl<CallTransition*>& newCalls
) {
    LLVM_DEBUG(
        llvm::dbgs() << " Inlining call " << *call
            << " edge " << call->getSource()->getId()
            << " --> " << call->getTarget()->getId()
            << "\n";
    );

    const CallInfo& info = mCalls[call];
    size_t topoIdx = mTopo.indexOf(call->getTarget());

    auto oldTopo = mTopoSortMap.find(call->getCalledAutomaton());
    assert(oldTopo != mTopoSortMap.end());

    CfaInlineResult result = InlineCall(call, mError, mErrorFieldVariable, suffix.str());

    // Add the new locations to the topological sort.
    // As every inlined location should come between the source and target of the original call transition,
    // we will insert them there in the topo sort.
    auto getInlinedLocation = [&result](Location* loc) {
        return result.locToLocMap[loc];
    };
    mTopo.insert(topoIdx,
         llvm::map_iterator(oldTopo->second.begin(), getInlinedLocation),
         llvm::map_iterator(oldTopo->second.end(), getInlinedLocation));

    // Add information for the newly inserted calls
    for (CallTransition* callEdge : result.newCalls) {
        mCalls[callEdge].callChain = info.callChain;
        mCalls[callEdge].callChain.push_back(callEdge->getCalledAutomaton());
        mCalls[callEdge].overApprox = mExprBuilder.False();
        newCalls.push_back(callEdge);
    }

    mInlinedLocations.insert(result.inlinedLocations.begin(), result.inlinedLocations.end());
    mInlinedVariables.insert(result.inlinedVariables.begin(), result.inlinedVariables.end());
}

auto BoundedModelCheckerImpl::checkBound(unsigned openCallSites, unsigned bound) -> std::unique_ptr<VerificationResult>
{
    if (openCallSites == 0) {
        // If we have no unhandled call sites, the program is guaranteed to be safe at this point.
        mStats.NumEndLocs = mRoot->getNumLocations();
        mStats.NumEndLocals = mRoot->getNumLocals();

        return VerificationResult::CreateSuccess();
    }

    if (bound == mSettings.maxBound) {
        // The maximum bound was reached.
        llvm::outs() << "Maximum bound is reached.\n";

        mStats.NumEndLocs = mRoot->getNumLocations();
        mStats.NumEndLocals = mRoot->getNumLocals();

        return VerificationResult::CreateBoundReached();
    }

    return nullptr;
}

auto BoundedModelCheckerImpl::runSolver() -> Solver::SolverStatus
{
    llvm::outs() << "    Running solver...\n";
    mTimer.start();
    auto status = mSolver->run();
    mTimer.stop();

    llvm::outs() << "      Elapsed time: ";
    mTimer.format(llvm::outs(), "s");
    llvm::outs() << "\n";
    mStats.SolverTime += mTimer.elapsed();

    return status;
}

void BoundedModelCheckerImpl::printStats(llvm::raw_ostream& os) const
{
    os << "--------- Statistics ---------\n";
    os << "Total solver time: ";
    llvm::format_provider<std::chrono::milliseconds>::format(mStats.SolverTime, os, "s");
    os << "\n";
    os << "Number of inlined procedures: " << mStats.NumInlined << "\n";
    os << "Number of locations on start: " << mStats.NumBeginLocs << "\n";
    os << "Number of locations on finish: " << mStats.NumEndLocs << "\n";
    os << "Number of variables on start: " << mStats.NumBeginLocals << "\n";
    os << "Number of variables on finish: " << mStats.NumEndLocals << "\n";
    os << "------------------------------\n";
    if (mSettings.printSolverStats) {
        mSolver->printStats(os);
    }
    os << "\n";
}

void BoundedModelCheckerImpl::dumpFormulaIfRequested(const ExprPtr& formula) const
{
    if (mSettings.dumpFormula) {
        formula->print(llvm::errs());
    }
}

void BoundedModelCheckerImpl::dumpSolverIfRequested() const
{
    if (mSettings.dumpSolver) {
        mSolver->dump(llvm::errs());
    }
}

void BoundedModelCheckerImpl::dumpAutomataIfRequested() const
{
    if (mSettings.debugDumpCfa) {
        for (const Cfa& cfa : mSystem) {
            cfa.view();
        }
    }
}
