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
#include "ThetaCfaGenerator.h"
#include "gazer/Core/Expr/ExprWalker.h"
#include "gazer/ADT/StringUtils.h"

#include <llvm/Support/raw_ostream.h>

#include <boost/range/irange.hpp>

#include <functional>
#include <utility>

using namespace gazer;

class ThetaExprPrinter : public ExprWalker<std::string>
{
public:
    explicit ThetaExprPrinter(std::function<std::string(Variable*)> replacedNames)
        : mReplacedNames(std::move(replacedNames))
    {}

    std::string print(const ExprPtr& expr) {
        return this->walk(expr);
    }

    /// If there was an expression which could not be handled by
    /// this walker, returns it. Otherwise returns nullptr.
    ExprPtr getInvalidExpr() {
        return mUnhandledExpr;
    }

protected:
    std::string visitExpr(const ExprPtr& expr) override {
        mUnhandledExpr = expr;
        llvm::errs() << "Unhandled expr " << *expr << "\n";
        return "__UNHANDLED_EXPR__";
    }

    std::string visitLiteral(const ExprRef<LiteralExpr>& expr) override
    {
        if (auto* intLit = llvm::dyn_cast<IntLiteralExpr>(expr)) {
            auto val = intLit->getValue();
            return val < 0 ? "(" + std::to_string(val) + ")" : std::to_string(intLit->getValue());
        }

        if (auto* boolLit = llvm::dyn_cast<BoolLiteralExpr>(expr)) {
            return boolLit->getValue() ? "true" : "false";
        }

        if (auto* realLit = llvm::dyn_cast<RealLiteralExpr>(expr)) {
            auto val = realLit->getValue();
            return std::to_string(val.numerator()) + "%" + std::to_string(val.denominator());
        }

        return visitExpr(expr);
    }

    std::string visitVarRef(const ExprRef<VarRefExpr>& expr) override {
        std::string newName = mReplacedNames(&expr->getVariable());
        if (!newName.empty()) {
            return newName;
        }

        return expr->getVariable().getName();
    }

    std::string visitAnd(const ExprRef<AndExpr>& expr) override
    {
        std::string buffer;
        llvm::raw_string_ostream rso{buffer};

        rso << "(";
        auto r = boost::irange<size_t>(0, expr->getNumOperands());

        join_print_as(rso, r.begin(), r.end(), " and ", [this](auto& rso, size_t i) {
            rso << getOperand(i);
        });
        rso << ")";

        return rso.str();
    }

    std::string visitOr(const ExprRef<OrExpr>& expr) override
    {
        std::string buffer;
        llvm::raw_string_ostream rso{buffer};

        rso << "(";
        auto r = boost::irange<size_t>(0, expr->getNumOperands());

        join_print_as(rso, r.begin(), r.end(), " or ", [this](auto& rso, size_t i) {
            rso << getOperand(i);
        });
        rso << ")";


        return rso.str();
    }

    std::string visitNot(const ExprRef<NotExpr>& expr) override {
        return "(not " + getOperand(0) + ")";
    }

#define PRINT_BINARY_INFIX(NAME, OPERATOR)                                                  \
    std::string visit##NAME(const ExprRef<NAME##Expr>& expr) override                       \
    {                                                                                       \
        return "(" + (getOperand(0)) + " " + (OPERATOR) + " " + (getOperand(1)) + ")";      \
    }

    PRINT_BINARY_INFIX(Add, "+")
    PRINT_BINARY_INFIX(Sub, "-")
    PRINT_BINARY_INFIX(Mul, "*")
    PRINT_BINARY_INFIX(Div, "/")
    PRINT_BINARY_INFIX(Mod, "mod")
    PRINT_BINARY_INFIX(Imply, "imply")

    PRINT_BINARY_INFIX(Eq, "=")
    PRINT_BINARY_INFIX(NotEq, "/=")
    PRINT_BINARY_INFIX(Lt, "<")
    PRINT_BINARY_INFIX(LtEq, "<=")
    PRINT_BINARY_INFIX(Gt, ">")
    PRINT_BINARY_INFIX(GtEq, ">=")

#undef PRINT_BINARY_INFIX

    std::string visitSelect(const ExprRef<SelectExpr>& expr) override {
        return "(if " + getOperand(0) + " then " + getOperand(1) + " else " + getOperand(2) + ")";
    }

    std::string visitArrayRead(const ExprRef<ArrayReadExpr>& expr) override {
        return  "(" + getOperand(0) + ")[" + getOperand(1) + "]";
    }

    std::string visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr) override {
        return  "(" + getOperand(0) + ")[" + getOperand(1) + " <- " + getOperand(2) + "]";
    }

private:
    ExprPtr mUnhandledExpr = nullptr;
    std::function<std::string(Variable*)> mReplacedNames;
};

std::string gazer::theta::printThetaExpr(const ExprPtr& expr)
{
    auto getName = [](Variable* var) -> std::string { return var->getName(); };
    return printThetaExpr(expr, getName);
}

std::string gazer::theta::printThetaExpr(const ExprPtr& expr, std::function<std::string(Variable*)> variableNames)
{
    ThetaExprPrinter printer(std::move(variableNames));

    return printer.print(expr);
}