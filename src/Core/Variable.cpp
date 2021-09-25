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
#include "GazerContextImpl.h"

#include "gazer/Core/Expr.h"

using namespace gazer;

Variable::Variable(llvm::StringRef name, Type& type)
    : Decl(Decl::Variable, type), mName(name)
{
    mExpr = type.getContext().pImpl->Exprs.create<VarRefExpr>(this);
}

VarRefExpr::VarRefExpr(Variable* variable)
    : Expr(Expr::VarRef, variable->getType()), mVariable(variable)
{
}

bool Variable::operator==(const Variable &other) const
{
    if (&getContext() != &other.getContext()) {
        return false;
    }

    return mName == other.mName;
}

void VarRefExpr::print(llvm::raw_ostream& os) const {
    os << mVariable->getType().getName() << " " << mVariable->getName();
}

VariableAssignment::VariableAssignment(Variable *variable, ExprPtr value)
    : mVariable(variable), mValue(std::move(value))
{
    assert(mVariable != nullptr);
    assert(mValue != nullptr);
    assert(mVariable->getType() == mValue->getType());
}

bool VariableAssignment::operator==(const VariableAssignment& other) const {
    return mVariable == other.mVariable && mValue == other.mValue;
}

bool VariableAssignment::operator!=(const VariableAssignment& other) const {
    return !operator==(other);
}

Variable* VariableAssignment::getVariable() const { return mVariable; }
ExprPtr VariableAssignment::getValue() const { return mValue; }

llvm::raw_ostream& gazer::operator<<(llvm::raw_ostream& os, const Variable& variable)
{
    os << variable.getType() << " " << variable.getName();
    return os;
}

llvm::raw_ostream& gazer::operator<<(llvm::raw_ostream& os, const VariableAssignment& va)
{
    va.print(os);
    return os;
}

void VariableAssignment::print(llvm::raw_ostream &os) const
{
    os << mVariable->getName() << " := " << *mValue;
}


