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

#include "ThetaCfaProcedureGenerator.h"
#include "ThetaCommon.h"

using namespace gazer;
using namespace gazer::theta;

using llvm::dyn_cast;

void ThetaCfaGenerator::write(llvm::raw_ostream& os, ThetaNameMapping& nameTrace, bool xcfa)
{
    if (!xcfa) {
        assert(mSystem.getNumAutomata() == 1 && "CFA supports only one automaton.");
    }

    // Create a closure to test variable names
    auto isValidVarName = [&nameTrace](const std::string& name) -> bool {
      // The variable name should not be present in the variable list.
      return std::find_if(nameTrace.variables.begin(), nameTrace.variables.end(), [name](auto& v1) {
        return name == v1.second->getName();
      }) == nameTrace.variables.end();
    };

    llvm::DenseMap<Variable*, std::unique_ptr<ThetaVarDecl>> globalVars;

    // Add variables
    for (auto& variable : mSystem.globals()) {
        auto name = validName(variable.getName(), isValidVarName);
        auto type = typeName(variable.getType());

        nameTrace.variables[name] = &variable;
        globalVars.try_emplace(&variable, std::make_unique<ThetaVarDecl>(name, type));
    }

    os << "main process __gazer_main_process {\n";

    for (const auto& global : globalVars) {
        os << "    ";
        global.second->print(os);
    }

    for (auto& cfa : mSystem.automata()) {
        ThetaCfaProcedureGenerator(&cfa, isValidVarName, globalVars).write(os, nameTrace, xcfa);
    }
    os << "}";
    os.flush();
}
