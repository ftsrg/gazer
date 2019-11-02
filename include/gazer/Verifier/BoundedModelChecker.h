//==- BoundedModelChecker.h - BMC engine interface --------------*- C++ -*--==//
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
///
/// \file This file declares the BoundedModelChecker verification backend,
/// along with its possible configuration settings.
///
//===----------------------------------------------------------------------===//
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
