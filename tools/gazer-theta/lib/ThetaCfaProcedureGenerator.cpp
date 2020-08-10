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
#include "ThetaCfaProcedureGenerator.h"
#include "gazer/Automaton/CfaTransforms.h"
#include <llvm/ADT/Twine.h>

using namespace llvm;
using namespace gazer::theta;

void gazer::ThetaCfaProcedureGenerator::writeCFA(llvm::raw_ostream& os, gazer::theta::ThetaNameMapping& nameTrace) {
    // this should not be needed, but it does some extra stuff related to error handling
    auto recursiveToCyclicResult = TransformRecursiveToCyclic(cfa);

    nameTrace.errorLocation = recursiveToCyclicResult.errorLocation;
    nameTrace.errorFieldVariable = recursiveToCyclicResult.errorFieldVariable;
    nameTrace.inlinedLocations = std::move(recursiveToCyclicResult.inlinedLocations);
    nameTrace.inlinedVariables = std::move(recursiveToCyclicResult.inlinedVariables);

    llvm::DenseMap<Location*, std::unique_ptr<ThetaLocDecl>> locs;
    llvm::DenseMap<Variable*, std::unique_ptr<ThetaVarDecl>> vars;
    std::vector<std::unique_ptr<ThetaEdgeDecl>> edges;

    const auto& globals = mGlobals;
    // Create a closure to test variable names
    auto isValidVarName = [&vars, &globals](const std::string& name) -> bool {
      // The variable name should not be present in the variable list.
      return std::find_if(vars.begin(), vars.end(), [name](auto& v1) {
        return name == v1.second->getName();
      }) == vars.end() &&
             std::find_if(globals.begin(), globals.end(), [name](auto& v1) {
               return name == v1.second->getName();
             }) == globals.end();
    };

    // Add variables
    for (auto& variable : cfa->locals()) {
        auto name = validName(variable.getName(), isValidVarName);
        auto type = typeName(variable.getType());

        //// name should be "result" if it is the output instead of the original (<func>_RES_VAR)
        //if (std::find(cfa->outputs().begin(), cfa->outputs().end(), variable) != cfa->outputs().end()) {
        //    name = "result";
        //}

        nameTrace.variables[name] = &variable;
        vars.try_emplace(&variable, std::make_unique<ThetaVarDecl>(name, type));
    }

    // inputs are defined elsewhere
    for (auto& variable : cfa->inputs()) {
        auto name = validName(variable.getName(), isValidVarName);
        auto type = typeName(variable.getType());

//        nameTrace.variables[name] = &variable;
        vars.try_emplace(&variable, std::make_unique<ThetaVarDecl>(name, type));
    }

    // Add locations
    for (Location* loc : cfa->nodes()) {
        ThetaLocDecl::Flag flag = ThetaLocDecl::Loc_State;
        if (loc == nameTrace.errorLocation) {
            flag = ThetaLocDecl::Loc_Error;
        } else if (cfa->getEntry() == loc) {
            flag = ThetaLocDecl::Loc_Init;
        } else if (cfa->getExit() == loc) {
            flag = ThetaLocDecl::Loc_Final;
        }

        auto locName = "loc" + std::to_string(loc->getId());

        nameTrace.locations[locName] = loc;
        locs.try_emplace(loc, std::make_unique<ThetaLocDecl>(locName, flag));
    }

    auto find_var = [&vars, &globals](Variable* var) -> const std::unique_ptr<ThetaVarDecl>& {
      auto it = vars.find(var);
      if (it == vars.end()) {
          return globals.find(var)->getSecond();
      }
      return it->getSecond();
    };

    // Add edges
    for (Transition* edge : cfa->edges()) {
        ThetaLocDecl& source = *locs[edge->getSource()];
        ThetaLocDecl& target = *locs[edge->getTarget()];

        std::vector<ThetaStmt> stmts;

        if (edge->getGuard() != BoolLiteralExpr::True(edge->getGuard()->getContext())) {
            stmts.push_back(ThetaStmt::Assume(edge->getGuard()));
        }

        if (auto assignEdge = dyn_cast<AssignTransition>(edge)) {
            for (const auto& assignment : *assignEdge) {
                auto lhsName = find_var(assignment.getVariable())->getName(); // TODO isn't this canonizeName()?

                if (llvm::isa<UndefExpr>(assignment.getValue())) {
                    stmts.push_back(ThetaStmt::Havoc(lhsName));
                } else {
                    stmts.push_back(ThetaStmt::Assign(lhsName, assignment.getValue()));
                }
            }
        } else if (auto callEdge = dyn_cast<CallTransition>(edge)) {
            assert(callEdge->getNumOutputs() <= 1 && "calls should have at most one output");

            llvm::SmallVector<VariableAssignment, 5> inputs;
            for (const auto& input : callEdge->inputs()) {
                auto lhsName = input.getVariable()->getName();

                auto rhs = input.getValue();
                static int paramCounter = 0;
                // Create a new variable because XCFA needs it.
                auto newVarName = "call_param_tmp_" + llvm::Twine(paramCounter++);

                auto variable = cfa->createLocal(newVarName.str(), rhs->getType());
                auto name = validName(variable->getName(), isValidVarName);
                auto type = typeName(variable->getType());

                nameTrace.variables[name] = variable;
                vars.try_emplace(variable, std::make_unique<ThetaVarDecl>(name, type));

                // initialize the new variable
                stmts.push_back(ThetaStmt::Assign(name, rhs));
                inputs.push_back(VariableAssignment(input.getVariable(), variable->getRefExpr()));
            }
            std::optional<Variable*> result = {};
            if (callEdge->getNumOutputs() == 1) {
                result = callEdge->outputs().begin()->getVariable();
            }
            stmts.push_back(ThetaStmt::Call(callEdge->getCalledAutomaton()->getName(), inputs, result));
        }

        edges.emplace_back(std::make_unique<ThetaEdgeDecl>(source, target, std::move(stmts)));
    }

    const auto INDENT  = "    ";
    const auto INDENT2 = "        ";

    auto canonizeName = [&vars, &globals](Variable* variable) -> std::string {
      auto it = globals.find(variable);
      if (it != globals.end()) {
          return it->getSecond()->getName();
      }

      if (vars.count(variable) == 0) {
          return variable->getName();
      }
      return vars[variable]->getName();
    };

    if (cfa == mSystem.getMainAutomaton()) {
        os << "main ";
    }
    // TODO main -> xmain temp solution
    os << "procedure x" << cfa->getName() << "(";
    bool first = true;
    for (auto& input : cfa->inputs()) {
        if (first) {
            first = false;
        } else {
            os << ", ";
        }
        auto name = vars[&input]->getName();
        auto type = vars[&input]->getType();

        os << name << " : " << type;
    }
    os << ") {\n";
    for (auto& variable : cfa->locals()) {
        os << INDENT;
        vars[&variable]->print(os);
        os << "\n";
    }

    for (Location* loc : cfa->nodes()) {
        os << INDENT;
        locs[loc]->print(os);
        os << "\n";
    }

    for (auto& edge : edges) {
        os << INDENT << edge->mSource.mName << " -> " << edge->mTarget.mName << " {\n";
        for (auto& stmt : edge->mStmts) {
            os << INDENT2;
            PrintVisitor visitor(os, canonizeName);
            std::visit(visitor, stmt.mContent);
            os << "\n";
        }
        os << INDENT << "}\n";
        os << "\n";
    }

    os << "}\n";
    os.flush();
}
