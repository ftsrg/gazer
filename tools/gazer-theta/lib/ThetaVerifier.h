//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
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
