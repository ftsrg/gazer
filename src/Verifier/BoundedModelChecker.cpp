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

#include "gazer/Support/Stopwatch.h"

#include <llvm/ADT/DenseSet.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/DepthFirstIterator.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Debug.h>

#include <boost/dynamic_bitset.hpp>

#include <sstream>

#define DEBUG_TYPE "BoundedModelChecker"

using namespace gazer;

namespace
{
    llvm::cl::opt<bool> NoDomPush("no-dom-push", llvm::cl::Hidden);
    llvm::cl::opt<bool> NoPostDomPush("no-postdom-push", llvm::cl::Hidden);
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
    // TODO: Clone the main automaton instead of modifying the original.
    mRoot = mSystem.getMainAutomaton();
    assert(mRoot != nullptr && "The main automaton must exist!");
}

void BoundedModelCheckerImpl::createTopologicalSorts()
{
    for (Cfa& cfa : mSystem) {
        auto& topoVec = mTopoSortMap[&cfa];
        createTopologicalSort(cfa, topoVec);
    }

    auto& mainTopo = mTopoSortMap[mRoot];
    mTopo.insert(mTopo.end(), mainTopo.begin(), mainTopo.end());

    for (size_t i = 0; i < mTopo.size(); ++i) {
        mLocNumbers[mTopo[i]] = i;
    }
}

auto BoundedModelCheckerImpl::initializeErrorField() -> bool
{
    // Set the verification goal - a single error location.
    llvm::SmallVector<Location*, 1> errors;
    Type* errorFieldType = nullptr;
    for (auto& loc : mRoot->nodes()) {
        if (loc->isError()) {
            errors.push_back(loc.get());
            errorFieldType = &mRoot->getErrorFieldExpr(loc.get())->getType();
        }
    }

    if (errorFieldType == nullptr) {
        // Try to find the error field type from using another CFA.
        // Try to find a suitable error field type.
        for (Cfa& cfa : mSystem) {
            for (auto& err : cfa.errors()) {
                errorFieldType = &err.second->getType();
                goto ERROR_TYPE_FOUND;
            }
        }

        // There are no error calls in the system, it is safe by definition.
        return false;
    }
    
ERROR_TYPE_FOUND:

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
    for (auto& loc : mRoot->nodes()) {
        if (visited.count(&*loc) == 0) {
            mRoot->disconnectLocation(&*loc);
        }
    }

    mRoot->clearDisconnectedElements();
}

auto BoundedModelCheckerImpl::check() -> std::unique_ptr<VerificationResult>
{
    // Initialize error field
    bool hasErrorLocation = this->initializeErrorField();
    if (!hasErrorLocation) {
        llvm::outs() << "No error location is present or it was discarded by the frontend.\n";
        return VerificationResult::CreateSuccess();
    }

    // Do a pre-processing step: remove all locations from which the error
    // location is not reachable.
    this->removeIrrelevantLocations();

    // Create the topological sorts
    this->createTopologicalSorts();

    // Insert initial call approximations.
    for (auto& edge : mRoot->edges()) {
        if (auto call = llvm::dyn_cast<CallTransition>(edge.get())) {
            mCalls[call].overApprox = mExprBuilder.False();
            mCalls[call].callChain.push_back(call->getCalledAutomaton());
        }
    }

    if (mSettings.debugDumpCfa) {
        for (Cfa& cfa : mSystem) { cfa.view(); }
    }

    // Initialize the path condition calculator
    PathConditionCalculator pathConditions(
        mTopo, mExprBuilder,
        this->createLocNumberFunc(),
        [this](CallTransition* call) -> ExprPtr {
            return mCalls[call].overApprox;
        },
        [this](Location* l, ExprPtr e) {
            mPredecessors.insert(l, e);
        }
    );

    // Do eager unrolling, if requested
    if (mSettings.eagerUnroll > mSettings.maxBound) {
        llvm::errs() << "ERROR: Eager unrolling bound is larger than maximum bound.\n";
        return VerificationResult::CreateUnknown();
    }

    unsigned tmp = 0;
    for (size_t bound = 1; bound <= mSettings.eagerUnroll; ++bound) {
        llvm::outs() << "Eager iteration " << bound << "\n";
        mOpenCalls.clear();
        for (auto& [call, info] : mCalls) {
            if (info.getCost() <= bound) {
                mOpenCalls.insert(call);
            }
        }

        llvm::SmallVector<CallTransition*, 16> callsToInline;
        for (CallTransition* call : mOpenCalls) {
            inlineCallIntoRoot(call, mInlinedVariables, "_call" + llvm::Twine(tmp++), callsToInline);
            mCalls.erase(call);
        }
    }    

    mStats.NumBeginLocs = mRoot->getNumLocations();
    mStats.NumBeginLocals = mRoot->getNumLocals();

    Location* top = mRoot->getEntry();
    Location* bottom = mError;

    bool skipUnderApprox = false;
    
    // Let's do some verification.
    for (size_t bound = mSettings.eagerUnroll + 1; bound <= mSettings.maxBound; ++bound) {
        llvm::outs() << "Iteration " << bound << "\n";

        while (true) {
            unsigned numUnhandledCallSites = 0;
            ExprPtr formula;
            Solver::SolverStatus status = Solver::UNKNOWN;

            if (!skipUnderApprox) {
                llvm::outs() << "  Under-approximating.\n";

                for (auto& entry : mCalls) {
                    entry.second.overApprox = mExprBuilder.False();
                }

                formula = pathConditions.encode(top, bottom);

                this->push();
                llvm::outs() << "    Transforming formula...\n";
                if (mSettings.dumpFormula) {
                    formula->print(llvm::errs());
                }

                mSolver->add(formula);

                if (mSettings.dumpSolver) {
                    mSolver->dump(llvm::errs());
                }
                
                status = this->runSolver();

                if (status == Solver::SAT) {
                    llvm::outs() << "  Under-approximated formula is SAT.\n";
                    return this->createFailResult();
                }

                this->pop();
            }

            skipUnderApprox = false;

            // If the under-approximated formula was UNSAT, there is no feasible path from start
            // to the error location which does not involve a call. Find the lowest common dominator
            // of all calls, and set is as the start location. Similarly, we can calculate the
            // highest common post-dominator for the error location of all calls to update the
            // target state. These nodes are the lowest common ancestors (LCA) of the calls in
            // the (post-)dominator trees.
            llvm::outs() << "  Attempting to set new starting and target points...\n";
            auto lca = this->findCommonCallAncestor(top, bottom);

            this->push();
            if (lca.first != nullptr) {
                LLVM_DEBUG(llvm::dbgs() << "Found LCA, " << lca.first->getId() << ".\n");
                assert(lca.second != nullptr);

                mSolver->add(pathConditions.encode(top, lca.first));
                mSolver->add(pathConditions.encode(lca.second, bottom));

                // Run the solver and check whether top and bottom are consistent -- if not,
                // we can return that the program is safe as all possible error paths will
                // encode these program parts.
                status = this->runSolver();
    
                if (status == Solver::UNSAT) {
                    llvm::outs() << "    Start and target points are inconsitent, no errors are reachable.\n";
                    return VerificationResult::CreateSuccess();
                }

            } else {
                LLVM_DEBUG(llvm::dbgs() << "No calls present, LCA is " << top->getId() << ".\n");
                lca = { top, bottom };
            }

            // Now try to over-approximate.
            llvm::outs() << "  Over-approximating.\n";

            mOpenCalls.clear();
            for (auto& [call, info] : mCalls) {
                if (info.getCost() > bound) {
                    LLVM_DEBUG(
                        llvm::dbgs() << "  Skipping " << *call
                        << ": inline cost is greater than bound (" <<
                        info.getCost() << " > " << bound << ").\n"
                    );
                    info.overApprox = mExprBuilder.False();
                    ++numUnhandledCallSites;
                    continue;
                }

                info.overApprox = mExprBuilder.True();
                mOpenCalls.insert(call);
            }

            this->push();

            llvm::outs() << "    Calculating verification condition...\n";
            formula = pathConditions.encode(lca.first, lca.second);
            if (mSettings.dumpFormula) {
                formula->print(llvm::errs());
            }

            llvm::outs() << "    Transforming formula...\n";
            mSolver->add(formula);

            if (mSettings.dumpSolver) {
                mSolver->dump(llvm::errs());
            }

            status = this->runSolver();

            if (status == Solver::SAT) {
                llvm::outs() << "      Over-approximated formula is SAT.\n";
                llvm::outs() << "      Checking counterexample...\n";

                // We have a counterexample, but it may be spurious.
                auto model = mSolver->getModel();

                llvm::SmallVector<CallTransition*, 16> callsToInline;
                this->findOpenCallsInCex(*model, callsToInline);

                llvm::outs() << "    Inlining calls...\n";
                while (!callsToInline.empty()) {
                    CallTransition* call = callsToInline.pop_back_val();
                    llvm::outs() << "      Inlining " << call->getSource()->getId() << " --> "
                        << call->getTarget()->getId() << " "
                        << call->getCalledAutomaton()->getName() << "\n";
                    mStats.NumInlined++;

                    llvm::SmallVector<CallTransition*, 4> newCalls;
                    this->inlineCallIntoRoot(
                        call, mInlinedVariables, "_call" + llvm::Twine(tmp++), newCalls
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

                this->pop();
                top = lca.first;
                bottom = lca.second;
            } else if (status == Solver::UNSAT) {
                llvm::outs() << "  Over-approximated formula is UNSAT.\n";
                if (numUnhandledCallSites == 0) {
                    // If we have no unhandled call sites,
                    // the program is guaranteed to be safe at this point.
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

                // Try with an increased bound.
                llvm::outs() << "    Open call sites still present. Increasing bound.\n";
                this->pop();
                top = lca.first;
                bottom = lca.second;

                // Skip redundant under-approximation step - all calls in the system are
                // under-approximated with 'False', which will not change when we jump
                // back to the under-approximation step.
                skipUnderApprox = true;
                break;
            } else {
                llvm_unreachable("Unknown solver status.");
            }
        }
    }

    return VerificationResult::CreateBoundReached();
}

auto BoundedModelCheckerImpl::createLocNumberFunc()
    -> std::function<size_t(Location*)>
{
    return [this](Location* loc) {
        auto predIt = mLocNumbers.find(loc);
        assert(predIt != mLocNumbers.end()
            && "All locations must be present in the location map!");

        return predIt->second;
    };
}

auto BoundedModelCheckerImpl::findCommonCallAncestor(Location* fwd, Location* bwd)
    -> std::pair<Location*, Location*>
{
    // Calculate the closest common dominator for all call nodes
    std::vector<Transition*> targets;
    std::transform(mCalls.begin(), mCalls.end(), std::back_inserter(targets), [](auto& pair) {
        return pair.first;
    });

    Location* dom;
    Location* pdom;

    if (!NoDomPush) {
        dom = findLowestCommonDominator(targets, mTopo, this->createLocNumberFunc(), fwd);
    } else {
        dom = fwd;
    }

    if (!NoPostDomPush) {
        pdom = findHighestCommonPostDominator(targets, mTopo, this->createLocNumberFunc(), bwd);
    } else {
        pdom = bwd;
    }

    return { dom, pdom };
}

void BoundedModelCheckerImpl::findOpenCallsInCex(Model& model, llvm::SmallVectorImpl<CallTransition*>& callsInCex)
{
    ExprModelEvaluator eval{model};
    auto cex = bmc::BmcCex{mError, *mRoot, eval, mPredecessors};

    for (auto state : cex) {
        auto call = llvm::dyn_cast_or_null<CallTransition>(state.getOutgoingTransition());
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
    llvm::DenseMap<Variable*, Variable*>& vmap,
    const llvm::Twine& suffix,
    llvm::SmallVectorImpl<CallTransition*>& newCalls
) {
    LLVM_DEBUG(
        llvm::dbgs() << " Inlining call " << *call
            << " edge " << call->getSource()->getId()
            << " --> " << call->getTarget()->getId()
            << "\n";
    );

    CallInfo& info = mCalls[call];
    auto callee = call->getCalledAutomaton();

    llvm::DenseMap<Location*, Location*> locToLocMap;
    llvm::DenseMap<Variable*, Variable*> oldVarToNew;

    llvm::DenseMap<Transition*, Transition*> edgeToEdgeMap;

    VariableExprRewrite rewrite(mExprBuilder);

    // Clone all local variables into the parent
    for (Variable& local : callee->locals()) {
        LLVM_DEBUG(llvm::dbgs() << "Callee local " << local.getName() << "\n"); 
        if (!callee->isOutput(&local)) {
            auto varname = (local.getName() + suffix).str();
            auto newLocal = mRoot->createLocal(varname, local.getType());
            oldVarToNew[&local] = newLocal;
            vmap[newLocal] = &local;
            rewrite[&local] = newLocal->getRefExpr();
        }
    }

    for (Variable& input : callee->inputs()) {
        LLVM_DEBUG(llvm::dbgs() << "Callee input " << input.getName() << "\n");
        if (!callee->isOutput(&input)) {
            auto varname = (input.getName() + suffix).str();
            auto newInput = mRoot->createLocal(varname, input.getType());
            oldVarToNew[&input] = newInput;
            vmap[newInput] = &input;

            auto arg = call->getInputArgument(input);
            assert(arg.has_value()
                && "Each call input assignment must map to an input variable in callee!");
            rewrite[&input] = arg->getValue();
            //rewrite[&input] = newInput->getRefExpr();
        }
    }

    for (Variable& output : callee->outputs()) {
        auto argument = call->getOutputArgument(output);
        assert(argument.has_value() && "Every callee output should be assigned in a call transition!");

        auto newOutput = argument->getVariable();
        oldVarToNew[&output] = newOutput;
        vmap[newOutput] = &output;
        rewrite[&output] = newOutput->getRefExpr();
    }

    // Insert the locations
    for (auto& origLoc : callee->nodes()) {
        auto newLoc = mRoot->createLocation();
        locToLocMap[origLoc.get()] = newLoc;
        mInlinedLocations[newLoc] = origLoc.get();

        if (origLoc->isError()) {
            mRoot->createAssignTransition(newLoc, mError, mExprBuilder.True(), {
                { mErrorFieldVariable, callee->getErrorFieldExpr(origLoc.get()) }
            });
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
                [&oldVarToNew, &rewrite] (const VariableAssignment& origAssign) {
                    return VariableAssignment {
                        oldVarToNew[origAssign.getVariable()],
                        rewrite.walk(origAssign.getValue())
                    };
                }
            );

            newEdge = mRoot->createAssignTransition(
                source, target, rewrite.walk(assign->getGuard()), newAssigns
            );
        } else if (auto nestedCall = llvm::dyn_cast<CallTransition>(origEdge)) {
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
                    //llvm::errs() << origAssign.getVariable()->getName() << "\n";

                    Variable* newVar = oldVarToNew.lookup(origAssign.getVariable());
                    assert(newVar != nullptr && "All variables should be present in the variable map!");

                    return VariableAssignment{
                        newVar,
                        //rewrite.visit(origAssign.getValue())
                        origAssign.getValue()
                    };
                }
            );

            auto callEdge = mRoot->createCallTransition(
                source, target,
                rewrite.walk(nestedCall->getGuard()),
                nestedCall->getCalledAutomaton(),
                newArgs,
                newOuts
            );

            newEdge = callEdge;
            mCalls[callEdge].callChain = info.callChain;
            mCalls[callEdge].callChain.push_back(callEdge->getCalledAutomaton());
            mCalls[callEdge].overApprox = mExprBuilder.False();
            newCalls.push_back(callEdge);
        } else {
            llvm_unreachable("Unknown transition kind!");
        }

        edgeToEdgeMap[origEdge] = newEdge;
    }

    Location* before = call->getSource();
    Location* after  = call->getTarget();

    mRoot->createAssignTransition(before, locToLocMap[callee->getEntry()], call->getGuard());
    mRoot->createAssignTransition(locToLocMap[callee->getExit()], after , mExprBuilder.True());

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

    // Update the location numbers
    for (auto it = insertPos, ie = mTopo.end(); it != ie; ++it) {
        size_t idx = std::distance(mTopo.begin(), it);
        mLocNumbers[*it] = idx;
    }

    mRoot->disconnectEdge(call);
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

void BoundedModelCheckerImpl::printStats(llvm::raw_ostream& os)
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

