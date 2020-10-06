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
#include "ThetaCommon.h"

#include "gazer/Automaton/CfaTransforms.h"
#include "gazer/Core/Expr.h"

#include <llvm/ADT/Twine.h>
#include <llvm/ADT/DenseSet.h>

#include <llvm/Pass.h>

using namespace gazer::theta;
using namespace llvm;

void ThetaCfaProcedureGenerator::write(llvm::raw_ostream& os, ThetaNameMapping& nameTrace, bool xcfa) {
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
        auto name = validName(variable.getName(), isValidVarName);
        auto type = typeName(variable.getType());

        nameTrace.variables[name] = &variable;
        vars.try_emplace(&variable, std::make_unique<ThetaVarDecl>(name, type));
    }

    for (auto& variable : mCfa->inputs()) {
        auto name = validName(variable.getName(), isValidVarName);
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

    auto& globals = this->globals;

    auto find_var = [&vars, &globals](Variable* var) -> const std::unique_ptr<ThetaVarDecl>& {
      auto it = vars.find(var);
      if (it != vars.end()) {
          return it->getSecond();
      }

      auto git = globals.find(var);
      if (git != globals.end()) {
          return git->getSecond();
      }

      llvm_unreachable(("Variable cannot be found: " + var->getName()).c_str());
    };

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
                auto lhsName = find_var(assignment.getVariable())->getName();

                if (llvm::isa<UndefExpr>(assignment.getValue())) {
                    stmts.push_back(ThetaStmt::Havoc(lhsName));
                } else {
                    ExprPtr assignmentValue = assignment.getValue();
                    if (assignment.isStore()) {
                        std::string name;
                        if (assignment.getValue()->getKind() == Expr::VarRef) {
                            auto& var = ((VarRefExpr*)assignment.getValue().get())->getVariable();
                            name = find_var(&var)->getName();
                        } else {
                            auto* temp =
                                mCfa->createLocal(lhsName, assignment.getVariable()->getType());
                            name = validName(lhsName, isValidVarName);
                            auto type = typeName(assignment.getVariable()->getType());
                            vars.try_emplace(temp, std::make_unique<ThetaVarDecl>(name, type));
                        }
                        stmts.push_back(ThetaStmt::Store(lhsName, name, assignment.getOrdering()));
                    } else if (assignment.isLoad()) {
                        assert(assignment.getValue()->getKind() == Expr::VarRef
                               && "Load should \"load\" into a variable");
                        auto& var = ((VarRefExpr*)assignment.getValue().get())->getVariable();
                        auto name = find_var(&var)->getName();
                        stmts.push_back(ThetaStmt::Load(name, lhsName, assignment.getOrdering()));
                    } else {
                        stmts.push_back(ThetaStmt::Assign(lhsName, assignment.getValue()));
                    }
                }
            }
        } else if (auto* callEdge = dyn_cast<CallTransition>(edge)) {
            ThetaStmt::CallData data;
            assert(callEdge->getNumOutputs() <= 1 && "Only one output is supported for calls");
            data.functionName = gazer::theta::thetaName(callEdge->getCalledAutomaton());

            llvm::SmallVector<Variable*, 3> inputTmpVars;
            for (const auto& input : callEdge->inputs()) {
                auto* oldVar = input.getVariable();
                auto* variable = mCfa->createLocal(oldVar->getName(), oldVar->getType());
                auto name = validName(variable->getName(), isValidVarName);
                auto type = typeName(variable->getType());
                nameTrace.variables[name] = variable;
                vars.try_emplace(variable, std::make_unique<ThetaVarDecl>(name, type));
                inputTmpVars.emplace_back(variable);
                stmts.emplace_back(ThetaStmt::Assign(name, input.getValue()));
            }

            for (auto* variable : inputTmpVars) {
                data.parameters.push_back(vars[variable]->getName());
            }
            if (callEdge->getNumOutputs() == 1) {
                auto output = find_var(callEdge->output_begin()->getVariable())->getName();
                data.resultVar = output;
            } else {
                data.resultVar = "";
            }

            stmts.push_back(ThetaStmt::Call(data));
        }

        edges.emplace_back(std::make_unique<ThetaEdgeDecl>(source, target, std::move(stmts)));
    }

    auto canonizeName = [&vars, &find_var](Variable* variable) -> std::string {
      if (vars.count(variable) == 0) {
          return variable->getName();
      }

      return find_var(variable)->getName();
    };

    auto INDENT  = "    ";
    auto INDENT2 = "        ";

    if (xcfa) {
        INDENT = "        ";
        INDENT2 = "            ";

        os << "    ";
        if (mCfa->getParent().getMainAutomaton() == mCfa) {
            os << "main ";
        }
        os << "procedure " << gazer::theta::thetaName(mCfa) << "(";
        bool first = true;
        for (auto& input: mCfa->inputs()) {
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
    }

    if (xcfa) {

        // inputs are already defined
        for (auto& variable : mCfa->locals()) {
            os << INDENT;
            vars[&variable]->print(os);
            os << "\n";
        }

    } else {
        for (auto& variable : llvm::concat<Variable>(mCfa->inputs(), mCfa->locals())) {
            os << INDENT;
            vars[&variable]->print(os);
            os << "\n";
        }

    }

    for (Location* loc : mCfa->nodes()) {
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
    if (xcfa) {
        os << "    }\n";
    }
}

