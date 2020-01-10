//==- Expr.h - Core expression classes --------------------------*- C++ -*--==//
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
#ifndef GAZER_CORE_DECL_H
#define GAZER_CORE_DECL_H

#include "gazer/Core/Type.h"
#include "gazer/Core/ExprRef.h"

#include <llvm/ADT/StringMap.h>

namespace gazer
{

class DeclContext;
class VarRefExpr;

/// Base class for all declarations.
class Decl
{
public:
    enum DeclKind
    {
        Param,
        Variable,
        Function
    };

protected:
    Decl(DeclKind kind, Type& type)
        : mKind(kind), mType(type)
    {}

protected:
    const DeclKind mKind;
    Type& mType;
};

// Variables
//===----------------------------------------------------------------------===//

class Variable final : public Decl
{
    friend class GazerContext;
    friend class GazerContextImpl;

    Variable(llvm::StringRef name, Type& type);
public:
    Variable(const Variable&) = delete;
    Variable& operator=(const Variable&) = delete;

    bool operator==(const Variable& other) const;
    bool operator!=(const Variable& other) const { return !operator==(other); }

    std::string getName() const { return mName; }
    Type& getType() const { return mType; }
    ExprRef<VarRefExpr> getRefExpr() const { return mExpr; }

    [[nodiscard]] GazerContext& getContext() const { return mType.getContext(); }

private:
    std::string mName;
    ExprRef<VarRefExpr> mExpr;
};

/// Convenience class for representing assignments to variables.
class VariableAssignment final
{
public:
    VariableAssignment();
    VariableAssignment(Variable *variable, ExprPtr value);

    bool operator==(const VariableAssignment& other) const;
    bool operator!=(const VariableAssignment& other) const;

    Variable* getVariable() const;
    ExprPtr getValue() const;

    void print(llvm::raw_ostream& os) const;

private:
    Variable* mVariable;
    ExprPtr mValue;
};

llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const Variable& variable);
llvm::raw_ostream& operator<<(llvm::raw_ostream& os, const VariableAssignment& va);

} // end namespace gazer

#endif
