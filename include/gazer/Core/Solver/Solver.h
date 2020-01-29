//==- Solver.h - SMT solver interface ---------------------------*- C++ -*--==//
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
#ifndef GAZER_CORE_SOLVER_SOLVER_H
#define GAZER_CORE_SOLVER_SOLVER_H

#include "gazer/Core/Expr.h"

namespace gazer
{

class Model;

/// Base interface for all solvers.
class Solver
{
public:
    enum SolverStatus
    {
        SAT,
        UNSAT,
        UNKNOWN
    };

public:
    explicit Solver(GazerContext& context)
        : mContext(context)
    {}

    Solver(const Solver&) = delete;
    Solver& operator=(const Solver&) = delete;

    void add(const ExprPtr& expr)
    {
        assert(expr->getType().isBoolType() && "Can only add bool expressions to a solver.");
        mStatCount++;
        addConstraint(expr);
    }

    unsigned getNumConstraints() const { return mStatCount; }

    GazerContext& getContext() const { return mContext; }

    virtual void printStats(llvm::raw_ostream& os) = 0;
    virtual void dump(llvm::raw_ostream& os) = 0;

    virtual SolverStatus run() = 0;
    virtual std::unique_ptr<Model> getModel() = 0;

    virtual void reset() = 0;

    virtual void push() = 0;
    virtual void pop() = 0;

    virtual ~Solver() = default;

protected:
    virtual void addConstraint(ExprPtr expr) = 0;

    GazerContext& mContext;
private:
    unsigned mStatCount = 0;
};

/// Identifies an interpolation group.
using ItpGroup = unsigned;

/// Interface for interpolating solvers.
class ItpSolver : public Solver
{
    using ItpGroupMapTy = std::unordered_map<ItpGroup, llvm::SmallVector<ExprPtr, 1>>;
public:
    using Solver::Solver;

    void add(ItpGroup group, const ExprPtr& expr)
    {
        assert(expr->getType().isBoolType() && "Can only add bool expressions to a solver.");
        mGroupFormulae[group].push_back(expr);

        this->addConstraint(group, expr);
    }

    // Interpolant groups
    ItpGroup createItpGroup() { return mGroupId++; }

    using itp_formula_iterator = typename ItpGroupMapTy::mapped_type::iterator;
    itp_formula_iterator group_formula_begin(ItpGroup group) {
        return mGroupFormulae[group].begin();
    }
    itp_formula_iterator group_formula_end(ItpGroup group) {
        return mGroupFormulae[group].end();
    }

    /// Returns an interpolant for a given interpolation group.
    virtual ExprPtr getInterpolant(ItpGroup group) = 0;

protected:
    using Solver::addConstraint;
    virtual void addConstraint(ItpGroup group, ExprPtr expr) = 0;

private:
    unsigned mGroupId = 1;
    ItpGroupMapTy mGroupFormulae;
};

/// Base factory class for all solvers, used to create new Solver instances.
class SolverFactory
{
public:
    /// Creates a new solver instance with a given symbol table.
    virtual std::unique_ptr<Solver> createSolver(GazerContext& symbols) = 0;
};

}

#endif
