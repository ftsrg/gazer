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

using namespace gazer;

Variable::Variable(llvm::StringRef name, Type& type)
    : mName(name), mType(type)
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


