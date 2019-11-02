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
#include "gazer/Core/Expr/ExprUtils.h"

#include <numeric>

using namespace gazer;

unsigned gazer::ExprDepth(const ExprPtr& expr)
{
    if (expr->isNullary()) {
        return 1;
    }

    if (auto nn = llvm::dyn_cast<NonNullaryExpr>(expr.get())) {
        unsigned max = 0;
        for (auto& op : nn->operands()) {
            unsigned d = ExprDepth(op);
            if (d > max) {
                max = d;
            }
        }

        return 1 + max;
    }

    llvm_unreachable("An expression cannot be nullary and non-nullary at the same time!");
}
