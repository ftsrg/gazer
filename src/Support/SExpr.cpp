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
#include "gazer/Support/SExpr.h"

#include <llvm/Support/raw_ostream.h>

#include <cctype>
#include <stack>

using namespace gazer;

static sexpr::Value* doParse(llvm::StringRef& input)
{
    input = input.drop_while(&isspace);
    if (input.empty()) {
        // TODO
        llvm::errs() << "Empty input string!\n";
        return nullptr;
    }

    if (input.consume_front("(")) {
        std::vector<sexpr::Value*> slist;
        while (input.front() != ')') {
            slist.emplace_back(doParse(input));
        }

        input = input.drop_front();

        return sexpr::list(std::move(slist));
    } else {
        // It must be an atom
        size_t closePos = std::min(input.find_if([](char c) {
            return isspace(c) || c == '(' || c == ')';
        }), input.size() - 1);
        auto data = input.substr(0, closePos);

        input = input.drop_front(closePos);

        return sexpr::atom(data.str());
    }
}

std::unique_ptr<sexpr::Value> gazer::sexpr::parse(llvm::StringRef input)
{
    std::vector<sexpr::Value*> list;
    auto trimmed = input.trim();
    
    if (trimmed.front() != '(' && trimmed.back() != ')') {
        llvm::errs() << "Invalid s-expression format!\n";
        return nullptr;
    }

    Value* tree = doParse(trimmed);

    // Wrap the resulting raw pointer into a unique_ptr
    return std::unique_ptr<sexpr::Value>(tree);
}

auto gazer::sexpr::atom(llvm::StringRef data) -> sexpr::Value*
{
    return new sexpr::Value(data);
}

auto gazer::sexpr::list(std::vector<Value*> data) -> sexpr::Value*
{
    return new sexpr::Value(std::move(data));
}

void sexpr::Value::print(llvm::raw_ostream& os) const
{
    if (mData.index() == 0) {
        os << std::get<0>(mData);
    } else if (mData.index() == 1) {
        os << "(";
        auto& vec = std::get<1>(mData);
        for (size_t i = 0; i < vec.size() - 1; ++i) {
            vec[i]->print(os);
            os << " ";
        }
        vec.back()->print(os);
        os << ")";
    } else {
        llvm_unreachable("Unknown variant state!");
    }
}

sexpr::Value::~Value()
{
    if (std::holds_alternative<ListTy>(mData)) {
        for (Value* val : std::get<ListTy>(mData)) {
            delete val;
        }
    }
}

bool sexpr::Value::operator==(const gazer::sexpr::Value& rhs) const
{
    if (mData.index() != rhs.mData.index()) {
        return false;
    }

    if (mData.index() == 0) {
        return asAtom() == rhs.asAtom();
    } else if (mData.index() == 1) {
        auto& vec1 = asList();
        auto& vec2 = rhs.asList();
        return std::equal(vec1.begin(), vec1.end(), vec2.begin(), [](auto& v1, auto& v2) {
           return *v1 == *v2;
        });
    } else {
        llvm_unreachable("Invalid variant state!");
    }
}

