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
#include "ThetaCfaGenerator.h"
#include "ThetaCommon.h"

#include "gazer/Automaton/CfaTransforms.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/DenseSet.h>

#include <llvm/Pass.h>

using namespace gazer::theta;
using namespace llvm;

void ThetaCfaProcedureGenerator::write(llvm::raw_ostream& os, ThetaNameMapping& nameTrace) {
    auto recursiveToCyclicResult = TransformRecursiveToCyclic(mCfa);

    nameTrace.errorLocation = recursiveToCyclicResult.errorLocation;
    nameTrace.errorFieldVariable = recursiveToCyclicResult.errorFieldVariable;
    nameTrace.inlinedLocations = std::move(recursiveToCyclicResult.inlinedLocations);
    nameTrace.inlinedVariables = std::move(recursiveToCyclicResult.inlinedVariables);

    llvm::DenseMap<Location*, std::unique_ptr<ThetaLocDecl>> locs;
    llvm::DenseMap<Variable*, std::unique_ptr<ThetaVarDecl>> vars;
    std::vector<std::unique_ptr<ThetaEdgeDecl>> edges;

    // Add variables
    for (auto& variable : mCfa->locals()) {
        auto name = validName(variable.getName(), mIsValidVarName);
        auto type = typeName(variable.getType());

        nameTrace.variables[name] = &variable;
        vars.try_emplace(&variable, std::make_unique<ThetaVarDecl>(name, type));
    }

    for (auto& variable : mCfa->inputs()) {
        auto name = validName(variable.getName(), mIsValidVarName);
        auto type = typeName(variable.getType());

        nameTrace.variables[name] = &variable;
        vars.try_emplace(&variable, std::make_unique<ThetaVarDecl>(name, type));
    }

    // Add locations
    for (Location* loc : mCfa->nodes()) {
        ThetaLocDecl::Flag flag = ThetaLocDecl::Loc_State;
        if (loc == recursiveToCyclicResult.errorLocation) {
            flag = ThetaLocDecl::Loc_Error;
        } else if (mCfa->getEntry() == loc) {
            flag = ThetaLocDecl::Loc_Init;
        } else if (mCfa->getExit() == loc) {
            flag = ThetaLocDecl::Loc_Final;
        }

        auto locName = "loc" + std::to_string(loc->getId());

        nameTrace.locations[locName] = loc;
        locs.try_emplace(loc, std::make_unique<ThetaLocDecl>(locName, flag));
    }

    // Add edges
    for (Transition* edge : mCfa->edges()) {
        ThetaLocDecl& source = *locs[edge->getSource()];
        ThetaLocDecl& target = *locs[edge->getTarget()];

        std::vector<ThetaStmt> stmts;

        if (edge->getGuard() != BoolLiteralExpr::True(edge->getGuard()->getContext())) {
            stmts.push_back(ThetaStmt::Assume(edge->getGuard()));
        }

        if (auto assignEdge = dyn_cast<AssignTransition>(edge)) {
            for (auto& assignment : *assignEdge) {
                auto lhsName = vars[assignment.getVariable()]->getName();

                if (llvm::isa<UndefExpr>(assignment.getValue())) {
                    stmts.push_back(ThetaStmt::Havoc(lhsName));
                } else {
                    stmts.push_back(ThetaStmt::Assign(lhsName, assignment.getValue()));
                }
            }
        } else if (auto* callEdge = dyn_cast<CallTransition>(edge)) {
            ThetaStmt::CallData data;
            assert(callEdge->getNumOutputs() <= 1 && "Only one output is supported for calls");
            data.functionName = callEdge->getCalledAutomaton()->getName(); // TODO mangling?
            for (const auto& input : callEdge->inputs()) {
                auto param = vars[input.getVariable()]->getName();
                data.parameters.push_back(param);
            }
            if (callEdge->getNumOutputs() == 1) {
                auto output = vars[callEdge->output_begin()->getVariable()]->getName();
                data.resultVar = output;
            } else {
                data.resultVar = "";
            }

            stmts.push_back(ThetaStmt::Call(data));
        }

        edges.emplace_back(std::make_unique<ThetaEdgeDecl>(source, target, std::move(stmts)));
    }

    auto INDENT_SIZE  = 4;
    auto INNER_INDENT_SIZE = 8;

    auto canonizeName = [&vars](Variable* variable) -> std::string {
      if (vars.count(variable) == 0) {
          return variable->getName();
      }

      return vars[variable]->getName();
    };

    for (auto& variable : llvm::concat<Variable>(mCfa->inputs(), mCfa->locals())) {
        os.indent(INDENT_SIZE);
        vars[&variable]->print(os);
        os << "\n";
    }

    for (Location* loc : mCfa->nodes()) {
        os.indent(INDENT_SIZE);
        locs[loc]->print(os);
        os << "\n";
    }

    for (auto& edge : edges) {
        os.indent(INDENT_SIZE);
        os << edge->mSource.mName << " -> " << edge->mTarget.mName << " {\n";
        for (auto& stmt : edge->mStmts) {
            os.indent(INNER_INDENT_SIZE);

            StmtPrintVisitor visitor(os, canonizeName);
            std::visit(visitor, stmt.mContent);
            os << "\n";
        }
        os.indent(INDENT_SIZE);
        os << "}\n";
        os << "\n";
    }
}

