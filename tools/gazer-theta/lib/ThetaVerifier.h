#ifndef GAZER_TOOLS_GAZERTHETA_LIB_THETAVERIFIER_H
#define GAZER_TOOLS_GAZERTHETA_LIB_THETAVERIFIER_H

#include "gazer/Automaton/Cfa.h"
#include "gazer/Verifier/VerificationAlgorithm.h"

namespace gazer::theta
{

struct ThetaSettings
{
    // Environment
    std::string thetaCfaPath;
    std::string thetaLibPath;
    unsigned timeout = 0;
    std::string modelPath;

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
    explicit ThetaVerifier(ThetaSettings settings)
        : mSettings(std::move(settings))
    {}

    std::unique_ptr<VerificationResult> check(AutomataSystem& system, CfaTraceBuilder& traceBuilder) override;
private:
    ThetaSettings mSettings;
};

} // end namespace gazer

#endif
