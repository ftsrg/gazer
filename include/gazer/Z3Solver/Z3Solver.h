#ifndef _GAZER_CORE_Z3SOLVER_H
#define _GAZER_CORE_Z3SOLVER_H

#include "gazer/Core/Solver/Solver.h"

#include <z3++.h>
#include <unordered_map>

namespace gazer
{

class Z3Solver : public Solver
{
public:
    Z3Solver(SymbolTable& symbols)
        : Solver(symbols), mContext(), mSolver(mContext)
    {}

    virtual SolverStatus run() override;
    virtual Valuation getModel() override;

protected:
    virtual void addConstraint(ExprPtr expr) override;

protected:
    z3::context mContext;
    z3::solver mSolver;
    unsigned mTmpCount = 0;
};

class CachingZ3Solver final : public Z3Solver
{
public:
    using CacheMapT = std::unordered_map<const Expr*, Z3_ast>;
    using Z3Solver::Z3Solver;

protected:
    virtual void addConstraint(ExprPtr expr) override;

private:
    CacheMapT mCache;
};

class Z3SolverFactory : public SolverFactory
{
public:
    Z3SolverFactory(bool cache = true)
        : mCache(cache)
    {}

    virtual std::unique_ptr<Solver> createSolver(SymbolTable& symbols) override;

private:
    bool mCache;
};

}

#endif
