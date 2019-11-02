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
#include "gazer/Z3Solver/Z3Solver.h"

#include <llvm/Support/raw_ostream.h>

#include <iostream>

using namespace gazer;

int main(void)
{
    llvm::APInt five{32, static_cast<uint64_t>(-5)};
    llvm::errs() << five << "\n";
    llvm::APInt two{32, 2};
    llvm::errs() << two << "\n";

    llvm::errs() << five.ult(two) << "\n";
    llvm::errs() << five.slt(two) << "\n";

    llvm::errs() << llvm::APInt::getMaxValue(8).getSExtValue() << "\n";
}
