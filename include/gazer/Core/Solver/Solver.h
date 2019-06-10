#ifndef _GAZER_CORE_SOLVER_SOLVER_H
#define _GAZER_CORE_SOLVER_SOLVER_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/Valuation.h"

namespace gazer
{

/// Base interface for all solvers
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
    Solver(GazerContext& context)
        : mContext(context)
    {}

    Solver(const Solver&) = delete;
    Solver& operator=(const Solver&) = delete;

    template<class InputIterator>
    void add(InputIterator begin, InputIterator end) {
        for (InputIterator i = begin; i != end; ++i) {
            add(*i);
        }
    }

    void add(ExprPtr expr) {
        assert(expr->getType().isBoolType() && "Can only add bool expressions to a solver.");
        mStatCount++;
        addConstraint(expr);
    }

    unsigned getNumConstraints() const { return mStatCount; }

    GazerContext& getContext() const { return mContext; }

    virtual void printStats(llvm::raw_ostream& os) = 0;
    virtual void dump(llvm::raw_ostream& os) = 0;

    virtual SolverStatus run() = 0;
    virtual Valuation getModel() = 0;

    virtual void reset() = 0;

    virtual ~Solver() {}

protected:
    virtual void addConstraint(ExprPtr expr) = 0;

    GazerContext& mContext;
private:
    unsigned mStatCount = 0;
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
