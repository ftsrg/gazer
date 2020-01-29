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
#ifndef GAZER_CORE_MODEL_H
#define GAZER_CORE_MODEL_H

#include "gazer/Core/Expr/ExprEvaluator.h"

namespace gazer
{

class LiteralExpr;

class Model
{
public:
    /// Returns the value of a given variable declaration.
    virtual ExprRef<LiteralExpr> eval(Variable& variable) = 0;
    ExprRef<LiteralExpr> eval(Variable* variable) {
        return this->eval(*variable);
    }

    virtual ~Model() = default;

    virtual void dump(llvm::raw_ostream& os) = 0;
};

/// Evaluates expressions using a solver model.
class ExprModelEvaluator : public ExprEvaluatorBase
{
public:
    ExprModelEvaluator(Model& model)
        : mModel(model)
    {}

    ExprRef<LiteralExpr> getVariableValue(Variable& variable) override {
        return mModel.eval(variable);
    }
private:
    Model& mModel;
};

}

#endif
