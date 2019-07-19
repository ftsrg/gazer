#include "BoundedModelCheckerImpl.h"

#include "gazer/Core/Expr/ExprRewrite.h"
#include "gazer/Core/Expr/ExprEvaluator.h"

#include "gazer/Support/Stopwatch.h"

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/Support/Debug.h>
#include <llvm/ADT/DepthFirstIterator.h>

#include <boost/dynamic_bitset.hpp>

#include <sstream>

#define DEBUG_TYPE "BoundedModelChecker"

namespace gazer
{

llvm::cl::opt<unsigned> MaxBound("bound", llvm::cl::desc("Maximum iterations for the bounded model checker."), llvm::cl::init(1));
llvm::cl::opt<unsigned> EagerUnroll("eager-unroll", llvm::cl::desc("Eager unrolling bound."), llvm::cl::init(0));

llvm::cl::opt<bool> VerifierDebug("debug-verif", llvm::cl::desc("Print verifier debug info"));
llvm::cl::opt<bool> ViewCfa("view-cfa", llvm::cl::desc("View the generated CFA."));
llvm::cl::opt<bool> DumpCfa("debug-dump-cfa", llvm::cl::desc("Dump the generated CFA after each inlining step."));

llvm::cl::opt<bool> Trace("trace", llvm::cl::desc("Print counterexample traces to stdout."));

} // end namespace gazer

using namespace gazer;

std::unique_ptr<SafetyResult> BoundedModelChecker::check(AutomataSystem& system)
{
    auto builder = CreateFoldingExprBuilder(system.getContext());
    BoundedModelCheckerImpl impl{system, *builder, mSolverFactory, mTraceBuilder};

    auto result = impl.check();

    impl.printStats(llvm::outs());

    return result;
}

BoundedModelCheckerImpl::BoundedModelCheckerImpl(
    AutomataSystem& system, ExprBuilder& builder, SolverFactory& solverFactory,
    TraceBuilder<Location*>* traceBuilder
) : mSystem(system),
    mExprBuilder(builder),
    mSolver(solverFactory.createSolver(system.getContext())),
    mTraceBuilder(traceBuilder)
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

        // Create the topological sorts
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

        if (errors.size() == 0) {
            // If there are no error locations in the main automaton, they might still exist in a called CFA.
            // Create a dummy error location which we will use as a goal.
            mError = mRoot->createErrorLocation();
            mRoot->createAssignTransition(mRoot->getEntry(), mError, mExprBuilder.False());
            mLocNumbers[mError] = mTopo.size();
            mTopo.push_back(mError);
        } else if (errors.size() == 1) {
            // We have a single error location, let that be the verification goal.
            mError = errors[0];
        } else {
            // Create an error location which will be directly reachable from already existing error locations.
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
    for (size_t bound = 0; bound < EagerUnroll; ++bound) {
        mOpenCalls.clear();
        for (auto& entry : mCalls) {
            CallTransition* call = entry.first;
            CallInfo& info = entry.second;

            if (info.cost <= bound) {
                mOpenCalls.insert(call);
            }
        }

        for (CallTransition* call : mOpenCalls) {
            inlineCallIntoRoot(call, mInlinedVariables, "_call" + llvm::Twine(tmp++));
            mCalls.erase(call);
        }
    }    

    mStats.NumBeginLocs = mRoot->getNumLocations();
    Location* start = mRoot->getEntry();

    Stopwatch<> sw;

    // Let's do some verification.
    for (size_t bound = EagerUnroll + 1; bound <= MaxBound; ++bound) {
        llvm::outs() << "Iteration " << bound << "\n";

        while (true) {
            unsigned numUnhandledCallSites = 0;
            llvm::outs() << "  Under-approximating.\n";

            size_t errIdx = mLocNumbers[mError];
            ExprPtr formula = this->forwardReachableCondition(start, mError);

            this->push();
            llvm::outs() << "    Transforming formula...\n";
            mSolver->add(formula);

            llvm::outs() << "    Running solver...\n";

            sw.start();
            auto status = mSolver->run();
            sw.stop();

            llvm::outs() << "      Elapsed time: ";
            sw.format(llvm::outs(), "s");
            llvm::outs() << "\n";
            mStats.SolverTime += sw.elapsed();

            if (status == Solver::SAT) {
                llvm::outs() << "  Under-approximated formula is SAT.\n";
                //LLVM_DEBUG(formula->print(llvm::dbgs()));
                //mRoot->view();
                auto model = mSolver->getModel();

                if (Trace) {
                    // Recover the sequence of transitions leading to the error location.
                    std::vector<Location*> states;
                    std::vector<std::vector<VariableAssignment>> actions;

                    bool hasParent = true;
                    Location* current = mError;
                    
                    ExprEvaluator eval{model};

                    while (hasParent) {
                        states.push_back(current);

                        auto predResult = mPredecessors.get(current);
                        if (predResult) {
                            auto predLit = llvm::dyn_cast_or_null<BvLiteralExpr>(
                                eval.visit(predResult->second).get()
                            );
                            
                            assert((predLit != nullptr)
                                && "Pred values should be evaluatable as bitvector literals!");

                            size_t predId = predLit->getValue().getLimitedValue();
                            Location* source = mRoot->findLocationById(predId);

                            assert(source != nullptr && "Locations should be findable by their id!");

                            auto edge = std::find_if(
                                current->incoming_begin(),
                                current->incoming_end(),
                                [source](Transition* e) { return e->getSource() == source; }
                            );

                            //llvm::errs() << "Current " << current->getId() << " next " << predId << " loc " << source->getId() << "\n";

                            assert(edge != current->incoming_end()
                                && "There must be an edge between a location and its direct predecessor!");

                            auto assignEdge = llvm::dyn_cast<AssignTransition>(*edge);                            
                            assert(assignEdge != nullptr
                                && "BMC traces must contain only assign transitions!"
                            );

                            std::vector<VariableAssignment> traceAction;
                            for (const VariableAssignment& assignment : *assignEdge) {
                                Variable* variable = assignment.getVariable();
                                Variable* origVariable = mInlinedVariables.lookup(assignment.getVariable());
                                if (origVariable == nullptr) {
                                    // This variable was not inlined, just use the original one.
                                    origVariable = variable;
                                }

                                ExprRef<AtomicExpr> value;
                                
                                if (model.find(assignment.getVariable()) == model.end()) {
                                    value = UndefExpr::Get(variable->getType());
                                } else {
                                    value = eval.visit(variable->getRefExpr());
                                }

                                traceAction.emplace_back(origVariable, value);
                            }
                            actions.push_back(traceAction);

                            current = source;
                        } else {
                            hasParent = false;
                        }
                    }

                    std::reverse(states.begin(), states.end());
                    std::reverse(actions.begin(), actions.end());

                    assert(states.size() == actions.size() + 1);

                    for (size_t i = 0; i < actions.size(); ++i) {
                        llvm::outs() << "State " << states[i]->getId() << "\n";
                        for (VariableAssignment assign : actions[i]) {
                            llvm::outs() << "   " << assign << "\n";
                        }
                    }
                    llvm::outs() << "State " << states.back()->getId() << "\n";
                }

                return SafetyResult::CreateFail(0);
            }

            this->pop();

            // If the under-approximated formula was UNSAT, there is no feasible path from start to the error location
            // which does not involve a call. Find the lowest common ancestor of all existing calls, and set is as the
            // start location.
            // TODO: We should also delete locations which have no reachable call descendants.
            Location* lca = this->findCommonCallAncestor();
            llvm::errs() << "Common call ancestor is " << lca->getId() << " on topo position " << mLocNumbers[lca] << "\n";

            this->clearLocationsWithoutCallDescendants(mError);
            //Location* lca = start;

            this->push();
            mSolver->add(forwardReachableCondition(start, lca));

            // Now try to over-approximate.
            llvm::outs() << "  Over-approximating.\n";

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

                info.overApprox = mExprBuilder.True();
                mOpenCalls.insert(call);
            }

            this->push();

            formula = this->forwardReachableCondition(lca, mError);
            mSolver->add(formula);

            llvm::outs() << "    Running solver...\n";

            sw.start();
            status = mSolver->run();
            sw.stop();

            llvm::outs() << "      Elapsed time: ";
            sw.format(llvm::outs(), "s");
            llvm::outs() << "\n";
            mStats.SolverTime += sw.elapsed();

            if (status == Solver::SAT) {
                llvm::outs() << "      Over-approximated formula is SAT.\n";
                llvm::outs() << "      Checking counterexample....\n";
                // We have a counterexample, but it may be spurious.

                llvm::outs() << "    Inlining calls...\n";

                for (CallTransition* call : mOpenCalls) {
                    mStats.NumInlined++;
                    inlineCallIntoRoot(call, mInlinedVariables, "_call" + llvm::Twine(tmp++));
                    mCalls.erase(call);
                }
                mRoot->clearDisconnectedElements();

                mStats.NumEndLocs = mRoot->getNumLocations();
                if (DumpCfa) {
                    mRoot->view();
                }

                this->pop();
                start = lca;
            } else if (status == Solver::UNSAT) {
                llvm::outs() << "  Over-approximated formula is UNSAT.\n";
                if (numUnhandledCallSites == 0) {
                    // If we have no unhandled call sites,
                    // the program is guaranteed to be safe at this point.

                    return SafetyResult::CreateSuccess();
                }  else if (bound == MaxBound) {
                    llvm::outs() << "Maximum bound is reached.\n";
                    // The maximum bound was reached.
                    return SafetyResult::CreateSuccess();
                } else {
                    // Try with an increased bound.
                    llvm::outs() << "    Open call sites still present. Increasing bound.\n";
                    this->pop();
                    start = lca;
                    break;
                }
            } else {
                llvm_unreachable("Unknown solver status.");
            }
        }
    }

    return SafetyResult::CreateSuccess();
}

ExprPtr BoundedModelCheckerImpl::forwardReachableCondition(Location* source, Location* target)
{
    if (source == target) {
        return mExprBuilder.True();
    }

    size_t startIdx = mLocNumbers[source];
    size_t targetIdx = mLocNumbers[target];
    
    LLVM_DEBUG(
        llvm::dbgs()
        << "Calculating condition between "
        << source->getId() << "(topo: " << startIdx << ")"
        << " and "
        << target->getId() << "(topo: " << targetIdx << ")"
        << "\n");

    assert(startIdx < targetIdx && "The source location must be before the target in a topological sort!");
    assert(targetIdx < mTopo.size() && "The target index is out of range in the VC array!");

    std::vector<ExprPtr> dp(targetIdx - startIdx + 1);

    std::fill(dp.begin(), dp.end(), mExprBuilder.False());

    // The first location is always reachable from itself.
    dp[0] = mExprBuilder.True();

    for (size_t i = 1; i < dp.size(); ++i) {
        Location* loc = mTopo[i + startIdx];
        ExprVector exprs;

        llvm::SmallVector<std::pair<Transition*, size_t>, 16> preds;
        for (Transition* edge : loc->incoming()) {
            auto predIt = mLocNumbers.find(edge->getSource());
            assert(predIt != mLocNumbers.end()
                && "All locations must be present in the location map");

            size_t predIdx = predIt->second;
            assert(predIdx < i + startIdx
                && "Predecessors must be before block in a topological sort. "
                "Maybe there is a loop in the automaton?");

            if (predIdx >= startIdx) {
                // We are skipping the predecessors which are outside the region we are interested in.
                preds.push_back({edge, predIdx});
            }
        }

        ExprPtr predExpr = nullptr;
        Variable* predVar = nullptr;

        if (Trace) {
            if (preds.empty()) {
                predExpr = nullptr;
                predVar = nullptr;
            } else if (preds.size() == 1) {
                predExpr = mExprBuilder.BvLit(preds[0].first->getSource()->getId(), 32);
                mPredecessors.insert(loc, {predVar, predExpr});
            } else if (preds.size() == 2) {
                predVar = mSystem.getContext().createVariable(
                    "__gazer_pred_" + std::to_string(mTmp++),
                    BoolType::Get(mSystem.getContext())
                );

                unsigned first =  preds[0].first->getSource()->getId();
                unsigned second = preds[1].first->getSource()->getId();
                
                predExpr = mExprBuilder.Select(
                    predVar->getRefExpr(), mExprBuilder.BvLit(first, 32), mExprBuilder.BvLit(second, 32)
                );
                mPredecessors.insert(loc, {predVar, predExpr});
            } else {
                predVar = mSystem.getContext().createVariable(
                    "__gazer_pred_" + std::to_string(mTmp++),
                    BvType::Get(mSystem.getContext(), 32)
                );
                predExpr = predVar->getRefExpr();
                mPredecessors.insert(loc, {predVar, predExpr});
            }
        }
        
        for (size_t j = 0; j < preds.size(); ++j) {
            Transition* edge = preds[j].first;
            size_t predIdx = preds[j].second;

            ExprPtr predIdentification;
            if (!Trace || predVar == nullptr) {
                predIdentification = mExprBuilder.True();
            } else if (predVar->getType().isBoolType()) {
                predIdentification =
                    (j == 0 ? predVar->getRefExpr() : mExprBuilder.Not(predVar->getRefExpr()));
            } else {
                predIdentification = mExprBuilder.Eq(
                    predVar->getRefExpr(),
                    mExprBuilder.BvLit(preds[j].first->getSource()->getId(), 32)
                );
            }

            ExprPtr formula = mExprBuilder.And({
                dp[predIdx - startIdx],
                predIdentification,
                edge->getGuard()
            });

            if (auto assignEdge = llvm::dyn_cast<AssignTransition>(edge)) {
                ExprVector assigns;
                std::transform(assignEdge->begin(), assignEdge->end(), std::back_inserter(assigns), [this](const VariableAssignment& varAssign) {
                    return this->mExprBuilder.Eq(varAssign.getVariable()->getRefExpr(), varAssign.getValue());
                });

                if (!assigns.empty()) {
                    formula = mExprBuilder.And(formula, mExprBuilder.And(assigns));
                }
            } else if (auto callEdge = llvm::dyn_cast<CallTransition>(edge)) {
                formula = mExprBuilder.And(formula, mCalls[callEdge].overApprox);
            }

            exprs.push_back(formula);
        }

        if (!exprs.empty()) {
            dp[i] = mExprBuilder.Or(exprs);
        } else {
            dp[i] = mExprBuilder.False();
        }
    }

    return dp.back();
}

Location* BoundedModelCheckerImpl::findCommonCallAncestor()
{
    auto start = std::min_element(mCalls.begin(), mCalls.end(), [this](auto& a, auto& b) {
        return mLocNumbers[a.first->getSource()] < mLocNumbers[b.first->getSource()];
    });

    size_t firstIdx = mLocNumbers[start->first->getSource()];

    llvm::DenseSet<Location*> candidates;
    candidates.insert(mTopo.begin(), std::next(mTopo.begin(), firstIdx));

    for (auto& entry : mCalls) {
        CallTransition* call = entry.first;
        llvm::df_iterator_default_set<Location*, 32> visited;

        auto begin = llvm::idf_ext_begin(call->getSource(), visited);
        auto end = llvm::idf_ext_end(call->getSource(), visited);

        // Perform the DFS by iterating through the df_iterator
        while (begin != end) {
            ++begin;
        }

        // Remove ancestors which were not visited by the DFS
        auto it = candidates.begin(), ie = candidates.end();
        while (it != ie) {
            auto j = it++;
            if (visited.count(*j) == 0) {
                candidates.erase(j);
            }
        }
    }

    assert(candidates.size() > 0 && "There must be at least one valid candidate (the entry node)!");

    return *std::max_element(candidates.begin(), candidates.end(),  [this](Location* a, Location* b) {
        return mLocNumbers[a] < mLocNumbers[b];
    });
}

void BoundedModelCheckerImpl::clearLocationsWithoutCallDescendants(Location* target)
{
    size_t targetIdx = mLocNumbers[target];
    size_t siz = targetIdx + 1;

    // We are doing two passes: one forward and one backward.
    boost::dynamic_bitset<> fwd(siz);
    boost::dynamic_bitset<> bwd(siz);

    for (size_t i = 0; i < fwd.size(); ++i) {
        Location* loc = mTopo[i];

        for (Transition* edge : loc->incoming()) {
            auto predIt = mLocNumbers.find(edge->getSource());
            assert(predIt != mLocNumbers.end()
                && "All locations must be present in the location map");

            size_t predIdx = predIt->second;
            assert(predIdx < i
                && "Predecessors must be before block in a topological sort. "
                "Maybe there is a loop in the automaton?");

            fwd[i] = fwd[predIdx] || edge->isCall();
        }
    }

    llvm::errs() << "Doing backward " << bwd.size() << "\n";

    for (size_t i = bwd.size(); i-- > 0;) {
        Location* loc = mTopo[i];

        llvm::errs() << "[" << i << "] " << loc->getId() << "\n";
        for (Transition* edge : loc->outgoing()) {
            auto descIt = mLocNumbers.find(edge->getTarget());
            assert(descIt != mLocNumbers.end()
                && "All locations must be present in the location map");

            size_t descIdx = descIt->second;
            assert(descIdx > i
                && "Descendants must be after block in a topological sort. "
                "Maybe there is a loop in the automaton?");

            if (descIdx > targetIdx) {
                // We are skipping the descendants which are outside the region we are interested in.
                continue;
            }

            bwd[i] = bwd[descIdx] || edge->isCall();
        }
    }

    std::stringstream ss;
    ss << fwd << "\n" << bwd << "\n";
    for (size_t i = 0; i < mTopo.size(); ++i) {
        ss << mTopo[i]->getId() << ", ";
    }
    ss << "\n";

    llvm::DenseSet<Location*> toRemove;
    for (size_t i = 0; i < siz; ++i) {
        Location* loc = mTopo[i];
        if (!fwd[i] && !bwd[i]) {
            llvm::errs() << "I is " << i << " removing " << loc->getId() << "\n";
            toRemove.insert(loc);
        }
    }

    llvm::errs() << mTopo.size() << ": ";
    mTopo.erase(std::remove_if(mTopo.begin(), mTopo.end(), [&toRemove](Location* l) {
        return toRemove.count(l) != 0;
    }), mTopo.end());
    llvm::errs() << mTopo.size() << "\n";

    mLocNumbers.clear();
    for (size_t i = 0; i < mTopo.size(); ++i) {
        mLocNumbers[mTopo[i]] = i;
    }

    for (size_t i = 0; i < mTopo.size(); ++i) {
        ss << mTopo[i]->getId() << ", ";
    }
    ss << "\n";

    llvm::errs() << ss.str();
    for (Location* loc : toRemove) {
        mRoot->disconnectLocation(loc);
    }
    mRoot->clearDisconnectedElements();
}

void BoundedModelCheckerImpl::inlineCallIntoRoot(
    CallTransition* call,
    llvm::DenseMap<Variable*, Variable*>& vmap,
    llvm::Twine suffix
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

    ExprRewrite rewrite(mExprBuilder);

    // Clone all local variables into the parent
    for (Variable& local : callee->locals()) {
        if (!callee->isOutput(&local)) {
            auto varname = (local.getName() + suffix).str();
            auto newLocal = mRoot->createLocal(varname, local.getType());
            oldVarToNew[&local] = newLocal;
            vmap[newLocal] = &local;
            rewrite[&local] = newLocal->getRefExpr();
        }
    }

    for (size_t i = 0; i < callee->getNumInputs(); ++i) {
            Variable* input = callee->getInput(i);
        if (!callee->isOutput(input)) {
            auto varname = (input->getName() + suffix).str();
            auto newInput = mRoot->createInput(varname, input->getType());
            oldVarToNew[input] = newInput;
            vmap[newInput] = input;
            rewrite[input] = call->getInputArgument(i);
        }
    }

    for (size_t i = 0; i < callee->getNumOutputs(); ++i) {
        Variable* output = callee->getOutput(i);
        auto newOutput = call->getOutputArgument(i).getVariable();
        oldVarToNew[output] = newOutput;
        vmap[newOutput] = output;
        rewrite[output] = call->getOutputArgument(i).getVariable()->getRefExpr();
    }

    // Insert the locations
    for (auto& origLoc : callee->nodes()) {
        auto newLoc = mRoot->createLocation();
        locToLocMap[origLoc.get()] = newLoc;
        mInlinedLocations[newLoc] = origLoc.get();

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
                [&oldVarToNew, &rewrite] (const VariableAssignment& origAssign) {
                    return VariableAssignment {
                        oldVarToNew[origAssign.getVariable()],
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
                [&rewrite, &oldVarToNew](const VariableAssignment& origAssign) {
                    return VariableAssignment{
                        oldVarToNew[origAssign.getVariable()],
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

    // Update the location numbers
    for (auto it = insertPos, ie = mTopo.end(); it != ie; ++it) {
        size_t idx = std::distance(mTopo.begin(), it);
        mLocNumbers[*it] = idx;
    }

    mRoot->disconnectEdge(call);
}


void BoundedModelCheckerImpl::printStats(llvm::raw_ostream& os) {
    os << "--------- Statistics ---------\n";
    os << "Total solver time: ";
    llvm::format_provider<std::chrono::milliseconds>::format(mStats.SolverTime, os, "s");
    os << "\n";
    os << "Number of inlined procedures: " << mStats.NumInlined << "\n";
    os << "Number of locations on start: " << mStats.NumBeginLocs << "\n";
    os << "Number of locations on finish: " << mStats.NumEndLocs << "\n";
    os << "------------------------------\n";
}
