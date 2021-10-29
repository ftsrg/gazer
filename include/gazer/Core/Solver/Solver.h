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
        mExprStack.push_back(expr);
        addConstraint(expr);
    }

    size_t getNumConstraints() const
    {
        return mExprStack.size();
    }

    GazerContext& getContext() const { return mContext; }

    virtual void printStats(llvm::raw_ostream& os) = 0;
    virtual void dump(llvm::raw_ostream& os) = 0;

    virtual SolverStatus run() = 0;
    virtual std::unique_ptr<Model> getModel() = 0;

    virtual void reset()
    {
        this->doReset();
        mExprStack.clear();
        mScopes.clear();
    }

    virtual void push()
    {
        this->doPush();
        mScopes.push_back(mExprStack.size());
    }

    virtual void pop()
    {
        this->doPop();
        mExprStack.resize(mScopes.back());
        mScopes.pop_back();
    }

    virtual ~Solver() = default;

protected:
    virtual void addConstraint(ExprPtr expr) = 0;
    virtual void doReset() = 0;
    virtual void doPush() = 0;
    virtual void doPop() = 0;

    GazerContext& mContext;
private:
    std::vector<ExprPtr> mExprStack;
    std::vector<size_t> mScopes;
};

/// Base factory class for all solvers, used to create new Solver instances.
class SolverFactory
{
public:
    /// Creates a new solver instance with a given symbol table.
    virtual std::unique_ptr<Solver> createSolver(GazerContext& symbols) = 0;

    virtual ~SolverFactory() = default;
};

} // namespace gazer

#endif
