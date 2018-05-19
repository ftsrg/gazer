#ifndef _GAZER_CORE_SOLVER_SOLVER_H
#define _GAZER_CORE_SOLVER_SOLVER_H

#include "gazer/Core/Expr.h"

namespace gazer
{

class Solver
{
public:
    enum SolverStatus {
        SAT, UNSAT, UNKNOWN
    };

public:
    Solver() = default;

    Solver(const Solver&) = delete;
    Solver& operator=(const Solver&) = delete;

    template<class It>
    void add(It begin, It end) {
        for (It i = begin; i != end; ++i) {
            add(*i);
        }
    }

    void add(ExprPtr expr) {
        assert(expr->getType().isBoolType() && "Can only add bool expressions to a solver.");
        addConstraint(expr);
    }

    virtual SolverStatus run() = 0;
    virtual ~Solver() {}
protected:
    virtual void addConstraint(ExprPtr expr) = 0;
};

}

#endif
