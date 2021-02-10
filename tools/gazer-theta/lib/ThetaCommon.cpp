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

#include "ThetaCommon.h"

namespace
{

constexpr std::array ThetaKeywords = {
    "main", "process", "var", "loc",
    "assume", "init", "final", "error",
    "return", "havoc", "bool", "int", "rat",
    "if", "then", "else", "iff", "imply",
    "forall", "exists", "or", "and", "not",
    "mod", "rem", "true", "false"
};

int tmpCount = 0;

} // namespace

std::string gazer::theta::validName(std::string name, std::function<bool(const std::string&)> isUnique)
{
    name = std::regex_replace(name, std::regex("[^a-zA-Z0-9_]"), "_");

    if (std::find(ThetaKeywords.begin(), ThetaKeywords.end(), name) != ThetaKeywords.end()) {
        name += "_gazer";
    }

    while (!isUnique(name)) {
        llvm::Twine nextTry = name + llvm::Twine(tmpCount++);
        name = nextTry.str();
    }

    return name;
}

void gazer::theta::ThetaStmt::print(llvm::raw_ostream& os) const
{
    PrintVisitor visitor(os, nullptr);

    std::visit(visitor, mContent);
}

void gazer::theta::ThetaEdgeDecl::print(llvm::raw_ostream& os) const
{
    os << mSource.mName << " -> " << mTarget.mName << " {\n";
    for (auto& stmt : mStmts) {
        os << "    ";
        stmt.print(os);
        os << "\n";
    }
    os << "}\n";
}

std::string gazer::theta::typeName(Type& type)
{
    switch (type.getTypeID()) {
    case Type::IntTypeID:
        return "int";
    case Type::RealTypeID:
        return "rat";
    case Type::BoolTypeID:
        return "bool";
    case Type::ArrayTypeID: {
        auto& arrTy = llvm::cast<ArrayType>(type);
        return "[" + typeName(arrTy.getIndexType()) + "] -> " + typeName(arrTy.getElementType());
    }
    default:
        llvm_unreachable("Types which are unsupported by theta should have been eliminated earlier!");
    }
}
