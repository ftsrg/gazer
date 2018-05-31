#ifndef _GAZER_CORE_SOLVER_SOLVER_H
#define _GAZER_CORE_SOLVER_SOLVER_H

#include "gazer/Core/Expr.h"

#include <iostream>

namespace gazer
{

class Solver
{
public:
    enum SolverStatus {
        SAT, UNSAT, UNKNOWN
    };

    class InsertIterator
        : public std::iterator<std::output_iterator_tag, void, void, void, void>
    {
    public:
        InsertIterator(Solver& solver)
            : mSolver(solver)
        {}
    public:
        InsertIterator& operator++() { return *this; }
        InsertIterator operator++(int) { return *this; }
        InsertIterator& operator*() { return *this; }

        InsertIterator& operator=(ExprPtr value) {
            mSolver.add(value);
            return *this;
        }
    private:
        Solver& mSolver;
    };
public:
    Solver() = default;

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
        addConstraint(expr);
    }

    void dump(std::ostream& os);

    virtual SolverStatus run() = 0;

    virtual ~Solver() {}
protected:
    virtual void addConstraint(ExprPtr expr) = 0;
};

}

#endif
