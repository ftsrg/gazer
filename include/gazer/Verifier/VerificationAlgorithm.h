#ifndef GAZER_VERIFIER_VERIFICATIONALGORITHM_H
#define GAZER_VERIFIER_VERIFICATIONALGORITHM_H

#include "gazer/Core/Expr.h"
#include "gazer/Trace/VerificationResult.h"

namespace gazer
{

class Location;
class AutomataSystem;

using CfaTraceBuilder = TraceBuilder<Location*, std::vector<VariableAssignment>>;

class VerificationAlgorithm
{
public:
    virtual std::unique_ptr<VerificationResult> check(
        AutomataSystem& system,
        CfaTraceBuilder& traceBuilder
    ) = 0;

    virtual ~VerificationAlgorithm() = default;
};

}

#endif
