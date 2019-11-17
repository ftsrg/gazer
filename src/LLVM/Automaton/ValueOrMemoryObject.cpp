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

#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/LLVM/Memory/MemoryObject.h"

using namespace gazer;

bool ValueOrMemoryObject::hasName() const
{
    struct HasValueVisitor {
        bool operator()(const llvm::Value* value) const { return value->hasName(); }
        bool operator()(const MemoryObjectDef* def) const {
            return !def->getObject()->getName().empty();
        }
    };

    return std::visit(HasValueVisitor{}, mVariant);
}

std::string ValueOrMemoryObject::getName() const
{
    struct GetNameVisitor {
        std::string operator()(const llvm::Value* value) const { return value->getName().str(); }
        std::string operator()(const MemoryObjectDef* def) const {
            return def->getName();
        }
    };

    return std::visit(GetNameVisitor{}, mVariant);
}