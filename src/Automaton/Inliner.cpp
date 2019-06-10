#include "gazer/Automaton/Inliner.h"

#include <llvm/ADT/Twine.h>

using namespace gazer;


void gazer::InlineCall(
    CallTransition* call,
    llvm::DenseMap<Variable*, Variable*>& vmap,
    llvm::StringRef suffix
) {
    Cfa* parent = call->getAutomaton();
    Cfa* callee = call->getCalledAutomaton();

    llvm::DenseMap<Location*, Location*> locToLocMap;
    llvm::DenseMap<Transition*, Transition*> edgeToEdgeMap;

    // Clone all local variables into the parent
    for (Variable& local : callee->locals()) {
        auto varname = local.getName() + suffix;
        vmap[&local] = parent->createLocal(varname.str(), local.getType());
    }
}

void inlineCallIntoRoot(
    CallEdge* call, std::string suffix,
    SummaryEdge** nextEdge,
    llvm::SmallVectorImpl<CallEdge*>& newCalls
) {
    CutPointGraph& proc = call->getCallee();
    llvm::DenseMap<CutPoint*, CutPoint*> cpToCpMap;
    llvm::DenseMap<CutPointEdge*, CutPointEdge*> edgeToEdgeMap;
    llvm::DenseMap<Variable*, Variable*> variableToVariableMap;
    llvm::DenseMap<Variable*, ExprPtr> variableRewriteMap;

    // Clone all local variables to the main procedure
    for (Variable* local : proc.locals()) {
        variableToVariableMap[local] = mRoot->addLocal(
            varname(local, suffix), local->getType()
        );
    }

    ExprVector inputsAssign, outputsAssign;
    this->inlineIoAssigns(
        call, suffix, inputsAssign, outputsAssign,
        variableToVariableMap, variableRewriteMap
    );

    llvm::DenseMap<Variable*, ExprPtr> rewriteMap;
    for (auto pair : variableToVariableMap) {
        rewriteMap[pair.first] = pair.second->getRefExpr();
    }
    rewriteMap.insert(variableRewriteMap.begin(), variableRewriteMap.end());

    ExprRewrite rewrite(rewriteMap);
    auto rewriteLamda = [&rewrite](const ExprPtr& expr) { return rewrite.transform(expr); };

    for (auto& originalCp : proc.nodes()) {
        cpToCpMap[originalCp.get()] = mRoot->createCutPoint(originalCp->getKind());
    }

    for (auto& originalEdge : proc.edges()) {
        CutPointEdge* newEdge = nullptr;
        CutPoint* from = cpToCpMap[originalEdge->getSource()];
        CutPoint* to = cpToCpMap[originalEdge->getTarget()];

        if (originalEdge->getKind() == CutPointEdge::Edge_Summary) {
            auto origSumm = llvm::cast<SummaryEdge>(originalEdge.get());
            newEdge = mRoot->createSummaryEdge(
                from, to, rewrite.transform(origSumm->getExpr())
            );
        } else if (originalEdge->getKind() == CutPointEdge::Edge_Call) {
            auto origCall = llvm::cast<CallEdge>(originalEdge.get());
            ExprVector newArgs;
            std::vector<Variable*> newOuts;

            std::transform(
                origCall->arg_begin(), origCall->arg_end(),
                std::back_inserter(newArgs), rewriteLamda
            );
            std::transform(
                origCall->output_begin(), origCall->output_end(),
                std::back_inserter(newOuts), [&variableToVariableMap](Variable* variable) {
                    return variableToVariableMap[variable];
                }
            );

            auto newCall = mRoot->createCallEdge(
                from, to, &origCall->getCallee(),
                { newArgs.begin(), newArgs.end() },
                { newOuts.begin(), newOuts.end() }
            );
            
            // Increase the cost for the calls contained within this procedure.
            unsigned newCost = mCosts[call] + 1;
            mCosts[newCall] = newCost;

            newCalls.push_back(newCall);
        }

        edgeToEdgeMap[originalEdge.get()] = newEdge;
    }

    CutPoint* preInit = call->getSource();
    CutPoint* postExit = call->getTarget();

    mRoot->disconnectEdge(call);

    *nextEdge = mRoot->createSummaryEdge(
        preInit, cpToCpMap[proc.getEntryCP()], mExprBuilder->And(inputsAssign)
    );
    mRoot->createSummaryEdge(
        cpToCpMap[proc.getExitCP()], postExit, mExprBuilder->And(outputsAssign)
    );
}

void inlineIoAssigns(
    CallEdge* call, std::string suffix,
    ExprVector& inputsAssign, ExprVector& outputsAssign,
    llvm::DenseMap<Variable*, Variable*>& variableToVariableMap,
    llvm::DenseMap<Variable*, ExprPtr>& variableRewriteMap
)
{
    CutPointGraph& proc = call->getCallee();
    auto ii = proc.inputs_begin(); auto ie = proc.inputs_end();
    auto ai = call->arg_begin(); auto ae = call->arg_end();
    for (; ii != ie && ai != ae; ++ii, ++ai) {
        Variable* input = *ii;
        auto newInput = mRoot->addLocal(
            varname(input, suffix), input->getType()
        );

        if ((*ai)->getKind() == Expr::Literal) {
            variableRewriteMap[input] = *ai;
        } else {
            variableToVariableMap[input] = newInput;
            inputsAssign.push_back(
                mExprBuilder->Eq(newInput->getRefExpr(), *ai)
            );
        }
    }

    auto oi = proc.outputs_begin(); auto oe = proc.outputs_end();
    auto ci = call->output_begin(); auto ce = call->output_end();
    for (; oi != oe && ci != ce; ++oi, ++ci) {
        Variable* input = *oi;
        auto newInput = mRoot->addLocal(
            varname(input, suffix), input->getType()
        );
        variableToVariableMap[input] = newInput;
        outputsAssign.push_back(
            mExprBuilder->Eq(newInput->getRefExpr(), (*ci)->getRefExpr())
        );
    }
}