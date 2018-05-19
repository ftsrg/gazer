#ifndef _GAZER_CORE_Z3SOLVER_H
#define _GAZER_CORE_Z3SOLVER_H

#include "gazer/Core/Solver/Solver.h"

#include <z3++.h>

namespace gazer
{

class Z3Solver final : public Solver
{
public:
    Z3Solver()
        : mContext(), mSolver(mContext)
    {}

    virtual SolverStatus run() override;

protected:
    virtual void addConstraint(ExprPtr expr) override;

private:
    z3::context mContext;
    z3::solver mSolver;
};

}

#endif
