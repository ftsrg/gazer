#ifndef _GAZER_BMC_BMC_H
#define _GAZER_BMC_BMC_H

#include "gazer/Core/Automaton.h"
#include "gazer/Core/Solver/Solver.h"

#include <functional>

namespace gazer
{

class BoundedModelChecker
{
public:
    enum Status {
        STATUS_SAFE,
        STATUS_UNSAFE,
        STATUS_UNKNOWN,
        STATUS_ERROR
    };

public:
    BoundedModelChecker(std::function<bool(Location*)> criterion, unsigned bound, Solver* solver)
        : mCriterion(criterion), mBound(bound), mSolver(solver)
    {}

    Status check(Automaton& cfa);

private:
    std::function<bool(Location*)> mCriterion;
    unsigned mBound;
    Solver* mSolver;
};

}

#endif
