//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2020 Contributors to the Gazer project
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
#ifndef GAZER_THETACFAPROCEDUREGENERATOR_H
#define GAZER_THETACFAPROCEDUREGENERATOR_H

#include "ThetaUtil.h"

#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/Twine.h>
#include <variant>

namespace gazer {

class ThetaCfaProcedureGenerator
{
public:
    ThetaCfaProcedureGenerator(
        AutomataSystem& system,
        Cfa* cfa,
        const llvm::DenseMap<Variable*, std::unique_ptr<theta::ThetaVarDecl>>& globals)
        : mSystem(system), cfa(cfa), mGlobals(globals)
    {}

    void writeCFA(llvm::raw_ostream& os, gazer::theta::ThetaNameMapping& nameTrace);

private:
    AutomataSystem& mSystem;
    Cfa* cfa;
    const llvm::DenseMap<Variable*, std::unique_ptr<theta::ThetaVarDecl>>& mGlobals;
};

} // namespace gazer

#endif // GAZER_THETACFAPROCEDUREGENERATOR_H
