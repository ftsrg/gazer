#include "BoundedModelCheckerImpl.h"

#include <llvm/Support/raw_ostream.h>

using namespace gazer;

void bmc::cex_iterator::advance()
{
    auto pred = mCex.mPredecessors.get(mState.getLocation());
    if (!pred) {
        // No predcessor information is available, this was the end of the counterexample trace.
        mState = { nullptr, nullptr };
        return;
    }

    Location* current = mState.getLocation();
    //auto predLit = llvm::dyn_cast_or_null<BvLiteralExpr>(
    //    mCex.mEval.walk(pred->second).get()
    //);
    auto predLit = llvm::dyn_cast_or_null<IntLiteralExpr>(
        mCex.mEval.walk(pred->second).get()
    );
                            
    assert((predLit != nullptr) && "Pred values should be evaluatable as bitvector literals!");
    
    size_t predId = predLit->getValue();//.getLimitedValue();
    Location* source = mCex.mCfa.findLocationById(predId);

    assert(source != nullptr && "Locations should be findable by their id!");

    auto edge = std::find_if(
        current->incoming_begin(),
        current->incoming_end(),
        [source](Transition* e) { return e->getSource() == source; }
    );

    assert(edge != current->incoming_end()
        && "There must be an edge between a location and its direct predecessor!");

    mState = { source, *edge };
}
