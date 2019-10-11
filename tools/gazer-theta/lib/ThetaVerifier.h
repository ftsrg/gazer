#ifndef GAZER_TOOLS_GAZERTHETA_LIB_THETAVERIFIER_H
#define GAZER_TOOLS_GAZERTHETA_LIB_THETAVERIFIER_H

#include "gazer/Automaton/Cfa.h"
#include "gazer/Verifier/VerificationAlgorithm.h"

namespace gazer::theta
{

class ThetaSettings
{
public:
    static ThetaSettings initFromCommandLine();

public:
    // Environment
    std::string thetaCfaPath;
    std::string z3Path;
    unsigned timeout = 0;

    // Algorithm settings
    std::string domain;
    std::string refinement;
    std::string search;
    std::string precGranularity;
    std::string predSplit;
    std::string encoding;
    std::string maxEnum;
    std::string initPrec;
};

class ThetaVerifier : public VerificationAlgorithm
{
public:
    std::unique_ptr<VerificationResult> check(AutomataSystem& system, CfaTraceBuilder& traceBuilder) override;
};

} // end namespace gazer

#endif
