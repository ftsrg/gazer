#ifndef GAZER_VERIFIER_BOUNDEDMODELCHECKER_H
#define GAZER_VERIFIER_BOUNDEDMODELCHECKER_H

#include "gazer/Verifier/VerificationAlgorithm.h"

namespace gazer
{

class Location;
class SolverFactory;
class AutomataSystem;

class BoundedModelChecker : public VerificationAlgorithm
{
public:
    BoundedModelChecker(SolverFactory& solverFactory)
        : mSolverFactory(solverFactory)
    {}

    std::unique_ptr<VerificationResult> check(
        AutomataSystem& system,
        CfaTraceBuilder& traceBuilder
    ) override;

private:
    SolverFactory& mSolverFactory;
};

}

#endif
