#ifndef GAZER_VERIFIER_BOUNDEDMODELCHECKER_H
#define GAZER_VERIFIER_BOUNDEDMODELCHECKER_H

#include "gazer/Verifier/VerificationAlgorithm.h"

namespace gazer
{

class Location;
class SolverFactory;
class AutomataSystem;

class BoundedModelChecker : public VerificationAlgorithm<AutomataSystem>
{
public:
    BoundedModelChecker(
        SolverFactory& solverFactory,
        TraceBuilder<Location*, std::vector<VariableAssignment>>* traceBuilder = nullptr
    )
        : mSolverFactory(solverFactory), mTraceBuilder(traceBuilder)
    {}

    std::unique_ptr<SafetyResult> check(AutomataSystem& system) override;

private:
    SolverFactory& mSolverFactory;
    TraceBuilder<Location*, std::vector<VariableAssignment>>* mTraceBuilder;
};

}

#endif
