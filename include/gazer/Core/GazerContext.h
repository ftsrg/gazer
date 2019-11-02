//==- GazerContext.h - Lifetime management for gazer objects ----*- C++ -*--==//
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
///
/// \file This file defines GazerContext, a container for all unique types,
/// variables and expressions.
///
//===----------------------------------------------------------------------===//
#ifndef GAZER_CORE_GAZERCONTEXT_H
#define GAZER_CORE_GAZERCONTEXT_H

#include <llvm/ADT/StringRef.h>

namespace gazer
{

class Type;
class Variable;
class GazerContext;
class GazerContextImpl;

class GazerContext
{
public:
    GazerContext();

    GazerContext(const GazerContext&) = delete;
    GazerContext& operator=(const GazerContext&) = delete;

    ~GazerContext();

public:

    /// Declares a new variable with the given name and type,
    /// or the already declared instance if a variable already
    /// exists with the given name.
    Variable* variableDecl(llvm::StringRef name, Type& type);

    Variable *getVariable(llvm::StringRef name);
    Variable *createVariable(llvm::StringRef name, Type &type);

    void removeVariable(Variable* variable);

    void dumpStats(llvm::raw_ostream& os) const;

public:
    const std::unique_ptr<GazerContextImpl> pImpl;
};

inline bool operator==(const GazerContext& lhs, const GazerContext& rhs) {
    // We only consider two context objects equal if they are the same object.
    return &lhs == &rhs;
}

inline bool operator!=(const GazerContext& lhs, const GazerContext& rhs) {
    return !(lhs == rhs);
}

} // end namespace gazer

#endif
