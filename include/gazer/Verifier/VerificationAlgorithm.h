#ifndef GAZER_VERIFIER_VERIFICATIONALGORITHM_H
#define GAZER_VERIFIER_VERIFICATIONALGORITHM_H

#include "gazer/Trace/SafetyResult.h"

namespace gazer
{

class AutomataSystem;

class VerificationAlgorithm
{
public:
    virtual std::unique_ptr<SafetyResult> check(AutomataSystem& system) = 0;

    virtual ~VerificationAlgorithm() = default;
};

}

#endif
