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

#include "gazer/Automaton/Cfa.h"
#include "ThetaCfaGenerator.h"
#include "ThetaCommon.h"

using namespace gazer;

namespace gazer::theta {

class ThetaCfaProcedureGenerator
{
public:
    ThetaCfaProcedureGenerator(Cfa* cfa, std::function<bool(const std::string&)> isValidVarName,
                               llvm::DenseMap<Variable*, std::unique_ptr<ThetaVarDecl>>& globals)
    : mCfa(cfa), isValidVarName(isValidVarName), globals(globals)
    {}

    void write(llvm::raw_ostream& os, ThetaNameMapping& nameTrace);

private:
    Cfa* mCfa;
    llvm::DenseMap<Variable*, std::unique_ptr<ThetaVarDecl>>& globals;
    std::function<bool(const std::string&)> isValidVarName;
};

} // namespace gazer::theta

#endif // GAZER_THETACFAPROCEDUREGENERATOR_H
