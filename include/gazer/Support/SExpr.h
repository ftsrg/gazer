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
#ifndef GAZER_SUPPORT_SEXPR_H
#define GAZER_SUPPORT_SEXPR_H

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>

#include <variant>
#include <vector>

namespace gazer::sexpr
{

class Atom;
class List;

class Value
{
    using ListTy = std::vector<Value*>;
    using VariantTy = std::variant<std::string, ListTy>;
public:
    explicit Value(std::string str)
        : mData(str)
    {}

    explicit Value(std::vector<Value*> vec)
        : mData(std::move(vec))
    {}

    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;

    [[nodiscard]] bool isAtom() const { return mData.index() == 0; }
    [[nodiscard]] bool isList() const { return mData.index() == 1; }

    [[nodiscard]] llvm::StringRef asAtom() const { return std::get<0>(mData); }
    [[nodiscard]] const std::vector<Value*>& asList() const { return std::get<1>(mData); }

    bool operator==(const Value& rhs) const;

    void print(llvm::raw_ostream& os) const;

    ~Value();

private:
    VariantTy mData;
};

/// Creates a simple SExpr atom and returns a pointer to it.
/// It is the caller's responsibility to free the resulting SExpr node.
[[nodiscard]] Value* atom(llvm::StringRef data);

/// Creates an SExpr list and returns a pointer to it.
/// It is the caller's responsibility to free the resulting SExpr node.
[[nodiscard]] Value* list(std::vector<Value*> data);

std::unique_ptr<Value> parse(llvm::StringRef input);

}

#endif
