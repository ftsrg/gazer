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
#ifndef GAZER_Z3SOLVER_Z3SOLVER_H
#define GAZER_Z3SOLVER_Z3SOLVER_H

#include "gazer/Core/Solver/Solver.h"

namespace z3 {
    class context;
    class model;
    class expr;
} // end namespace z3

namespace gazer
{

class Z3SolverFactory : public SolverFactory
{
public:
    Z3SolverFactory() = default;

    std::unique_ptr<Solver> createSolver(GazerContext& context) override;
};

/// Utility function which transforms an arbitrary Z3 bitvector into LLVM's APInt.
llvm::APInt z3_bv_to_apint(z3::context& context, z3::model& model, const z3::expr& expr);

} // end namespace gazer

#endif
