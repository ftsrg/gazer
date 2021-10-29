//==- ExprUtils.h -----------------------------------------------*- C++ -*--==//
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
#ifndef GAZER_CORE_EXPR_EXPRUTILS_H
#define GAZER_CORE_EXPR_EXPRUTILS_H

#include "gazer/Core/Expr.h"

namespace gazer
{

void InfixPrintExpr(const ExprPtr& expr, llvm::raw_ostream& os, unsigned bvRadix = 10);

} // namespace gazer

#endif