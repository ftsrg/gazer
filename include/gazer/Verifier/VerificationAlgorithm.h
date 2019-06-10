#ifndef GAZER_VERIFIER_VERIFICATIONALGORITHM_H
#define GAZER_VERIFIER_VERIFICATIONALGORITHM_H

#include "gazer/Trace/SafetyResult.h"

namespace gazer
{

class VerificationAlgorithm
{
public:
    SafetyResult check();
};

}

#endif
