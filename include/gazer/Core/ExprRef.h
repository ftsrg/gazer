//==- Expr.h - Core expression classes --------------------------*- C++ -*--==//
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
//
/// \file This file contains a forward declaration for Expr and ExprRef.
//
//===----------------------------------------------------------------------===//
#ifndef GAZER_CORE_EXPRREF_H
#define GAZER_CORE_EXPRREF_H

#include <boost/intrusive_ptr.hpp>

namespace gazer
{

class Expr;

template<class T = Expr> using ExprRef = boost::intrusive_ptr<T>;
using ExprPtr = ExprRef<Expr>;

} // namespace gazer

#endif