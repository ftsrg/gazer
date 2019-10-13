#ifndef GAZER_VERIFIER_BOUNDEDMODELCHECKER_H
#define GAZER_VERIFIER_BOUNDEDMODELCHECKER_H

#include "gazer/Verifier/VerificationAlgorithm.h"

namespace gazer
{

class Location;
class SolverFactory;
class AutomataSystem;

struct BmcSettings
{
    // Environment
    bool trace;

    // Debug
    bool debugDumpCfa;

    bool dumpFormula;
    bool dumpSolver;
    bool dumpSolverModel;
    bool printSolverStats;

    // Algorithm settings
    unsigned maxBound;
    unsigned eagerUnroll;
    bool simplifyExpr;
};

class BoundedModelChecker : public VerificationAlgorithm
{
public:
    explicit BoundedModelChecker(SolverFactory& solverFactory, BmcSettings settings)
        : mSolverFactory(solverFactory), mSettings(settings)
    {}

    std::unique_ptr<VerificationResult> check(
        AutomataSystem& system,
        CfaTraceBuilder& traceBuilder
    ) override;

private:
    SolverFactory& mSolverFactory;
    BmcSettings mSettings;
};

}

#endif
