#ifndef GAZER_VERIFIER_BOUNDEDMODELCHECKER_H
#define GAZER_VERIFIER_BOUNDEDMODELCHECKER_H

#include "gazer/Verifier/VerificationAlgorithm.h"

namespace gazer
{

class Location;
class SolverFactory;

class BoundedModelChecker : public VerificationAlgorithm
{
public:
    BoundedModelChecker(SolverFactory& solverFactory, TraceBuilder<Location*>* traceBuilder = nullptr)
        : mSolverFactory(solverFactory), mTraceBuilder(traceBuilder)
    {}

    std::unique_ptr<SafetyResult> check(AutomataSystem& system) override;
private:
    SolverFactory& mSolverFactory;
    TraceBuilder<Location*>* mTraceBuilder;
};

}

#endif
