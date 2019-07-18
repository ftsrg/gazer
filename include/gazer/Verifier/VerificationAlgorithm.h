#ifndef GAZER_VERIFIER_VERIFICATIONALGORITHM_H
#define GAZER_VERIFIER_VERIFICATIONALGORITHM_H

#include "gazer/Trace/SafetyResult.h"

namespace gazer
{

template<class InputFormalism>
class VerificationAlgorithm
{
public:
    virtual std::unique_ptr<SafetyResult> check(InputFormalism& system) = 0;

    virtual ~VerificationAlgorithm() = default;
};

}

#endif
