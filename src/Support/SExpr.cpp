#include "gazer/Support/SExpr.h"

#include <llvm/Support/raw_ostream.h>

#include <cctype>
#include <stack>

using namespace gazer;

static std::unique_ptr<sexpr::Value> doParse(llvm::StringRef& input)
{
    input = input.drop_while(&isspace);
    if (input.empty()) {
        // TODO
        llvm::errs() << "Empty input string!\n";
        return nullptr;
    }

    if (input.consume_front("(")) {
        std::vector<std::unique_ptr<sexpr::Value>> slist;
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

    return doParse(trimmed);
}

auto gazer::sexpr::atom(std::string data) -> std::unique_ptr<sexpr::Value>
{
    return std::make_unique<sexpr::Value>(data);
}

auto gazer::sexpr::list(std::vector<std::unique_ptr<sexpr::Value>> data) -> std::unique_ptr<sexpr::Value>
{
    return std::make_unique<sexpr::Value>(std::move(data));
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

