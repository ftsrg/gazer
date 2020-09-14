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
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/ADT/APInt.h>

using namespace gazer;

ExprPtr ExprBuilder::createTupleConstructor(TupleType& type, const ExprVector& members)
{
    return TupleConstructExpr::Create(type, members);
}

ExprPtr ExprBuilder::TupleInsert(const ExprPtr &tuple, const ExprPtr &value, unsigned int index)
{
    assert(tuple->getType().isTupleType() && "Can only perform tuple insertion on tuples!");
    auto& tupleTy = llvm::cast<TupleType>(tuple->getType());

    assert(index <= tupleTy.getNumSubtypes() && "Tuple insert index out of range!");
    assert(tupleTy.getTypeAtIndex(index) == value->getType() && "Tuple insert of incompatible types!");

    ExprVector newMembers;
    newMembers.reserve(tupleTy.getNumSubtypes());

    if (auto tupleConstruct = llvm::dyn_cast<TupleConstructExpr>(tuple)) {
        newMembers.insert(newMembers.begin(), tupleConstruct->op_begin(), tupleConstruct->op_end());
    } else {
        newMembers.resize(tupleTy.getNumSubtypes());
        for (unsigned i = 0; i < tupleTy.getNumSubtypes(); ++i) {
            newMembers[i] = this->TupSel(tuple, i);
        }
    }

    newMembers[index] = value;
    return this->createTupleConstructor(tupleTy, newMembers);
}

ExprPtr ExprBuilder::BvResize(const ExprPtr &op, BvType &type)
{
    assert(op->getType().isBvType() && "BvResize only works on bit-vectors!");
    unsigned width = llvm::cast<BvType>(op->getType()).getWidth();

    if (width < type.getWidth()) {
        return this->ZExt(op, type);
    }

    if (width > type.getWidth()) {
        return this->Trunc(op, type);
    }

    return op;
}

ExprPtr ExprBuilder::TupSel(const ExprPtr& tuple, unsigned index)
{
    return TupleSelectExpr::Create(tuple, index);
}

std::unique_ptr<ExprBuilder> gazer::CreateExprBuilder(GazerContext& context)
{
    return std::make_unique<ExprBuilder>(context);
}
