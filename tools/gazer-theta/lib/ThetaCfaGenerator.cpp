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
#include "ThetaCfaGenerator.h"
#include "gazer/Automaton/CfaTransforms.h"
#include "ThetaCfaProcedureGenerator.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/DenseSet.h>

using namespace gazer;
using namespace gazer::theta;

using llvm::dyn_cast;

void ThetaCfaGenerator::write(llvm::raw_ostream& os, ThetaNameMapping& nameTrace)
{
    llvm::DenseMap<Variable*, std::unique_ptr<ThetaVarDecl>> mGlobals;
    auto& globals = mGlobals;
    auto isValidVarName = [&globals](const std::string& name) -> bool {
      // The variable name should not be present in the variable list.
      return std::find_if(globals.begin(), globals.end(), [name](auto& v1) {
        return name == v1.second->getName();
      }) == globals.end();
    };

    for (auto& globalVar: mSystem.globals()) {
        auto name = validName(globalVar->getName(), isValidVarName);
        auto type = typeName(globalVar->getType());

        mGlobals.insert({globalVar, std::make_unique<ThetaVarDecl>(name, type)});
    }

    os << "main process main_process {\n";

    for (auto& globalVar : mGlobals) {
        os << "    ";
        globalVar.getSecond()->print(os);
        os << "\n";
    }

    for (Cfa& cfa : mSystem) {
        ThetaCfaProcedureGenerator(mSystem, &cfa, globals).writeCFA(os, nameTrace);
    }
    os << "}\n";
}
