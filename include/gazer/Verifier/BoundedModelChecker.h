#ifndef GAZER_VERIFIER_BOUNDEDMODELCHECKER_H
#define GAZER_VERIFIER_BOUNDEDMODELCHECKER_H

#include "gazer/Verifier/VerificationAlgorithm.h"

namespace gazer
{

class SolverFactory;

class BoundedModelChecker : public VerificationAlgorithm
{
public:
    BoundedModelChecker(SolverFactory& solverFactory)
        : mSolverFactory(solverFactory)
    {}

    std::unique_ptr<SafetyResult> check(AutomataSystem& system) override;
private:
    SolverFactory& mSolverFactory;
};

}

#endif
