#include "gazer/Automaton/CfaTransforms.h"
#include "gazer/Automaton/CallGraph.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/ADT/PostOrderIterator.h>

using namespace gazer;

namespace
{

class ErrorCodeInstrumentation
{
public:

private:
    Variable* transformAutomaton(Cfa* cfa);
    void transformCall(CallTransition* call);
};

} // end anonymous namespace

static Variable* transformAutomaton(Cfa* cfa)
{   
    if (cfa->getNumErrors() == 0) {
        // The automaton has no error calls -- nothing to do here.
        return nullptr;
    }

    auto& errorTy = cfa->error_begin()->second->getType();

    // Insert a new output variable for the error code.
    Variable* ec = cfa->createLocal("__gazer_error_code", errorTy);
    cfa->addOutput(ec);

    ExprPtr noError;
    if (auto bvTy = llvm::dyn_cast<BvType>(&errorTy)) {
        noError = BvLiteralExpr::Get(*bvTy, llvm::APInt{bvTy->getWidth(), 0});
    } else if (auto iTy = llvm::dyn_cast<IntType>(&errorTy)) {
        noError = IntLiteralExpr::Get(*iTy, 0);
    } else if (auto bTy = llvm::dyn_cast<BoolType>(&errorTy)) {
        noError = BoolLiteralExpr::False(*bTy);
    } else {
        llvm_unreachable("Invalid error code type. Valid types are: Bv, Int, Bool.");
    }

    // Set the error code variable to 0 for every 'normal' exit.
    for (auto exitEdge : cfa->getExit()->incoming()) {
        if (auto assignEdge = llvm::dyn_cast<AssignTransition>(exitEdge)) {
            assignEdge->addAssignment({ec, noError});
        }
    }

    for (auto error : cfa->errors()) {
        cfa->createAssignTransition(error.first, cfa->getExit(), BoolLiteralExpr::True(ec->getContext()), {
            { ec, error.second }
        });
    }

    return ec;
}

void gazer::PerformErrorCodeInstrumentation(AutomataSystem& system)
{
    llvm::DenseMap<Cfa*, Variable*> errorCodes;
    for (Cfa& cfa : system) {
        auto ec = transformAutomaton(&cfa);
        errorCodes[&cfa] = ec;
    }

    CallGraph cg(system);
    CallGraph::Node* main = cg.lookupNode(system.getMainAutomaton());

    auto cg_begin = llvm::po_begin(main);
    auto cg_end = llvm::po_end(main);

    for (auto it = cg_begin; it != cg_end; ++it) {
        Cfa* cfa = it->getCfa();

        for (auto& edge : cfa->edges()) {
            if (auto call = llvm::dyn_cast<CallTransition>(&*edge)) {
                Cfa* callee = call->getCalledAutomaton();
                if (callee->getNumErrors() == 0) {
                    continue;
                }
            }
        }
    }
}
