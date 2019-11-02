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
                            
    assert((predLit != nullptr) && "Pred values should be evaluatable as integer literals!");
    
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
