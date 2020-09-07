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
#include "ThetaType.h"
#include "gazer/Core/Expr/ExprWalker.h"
#include "gazer/ADT/StringUtils.h"

#include <llvm/Support/raw_ostream.h>

#include <boost/range/irange.hpp>

#include <bitset>
#include <functional>

using namespace gazer;
using namespace gazer::theta;

class ThetaExprPrinter : public ExprWalker<ThetaExprPrinter, std::string>
{
public:
    ThetaExprPrinter(std::function<std::string(Variable*)> replacedNames)
        : mReplacedNames(replacedNames)
    {}

    /// If there was an expression which could not be handled by
    /// this walker, returns it. Otherwise returns nullptr.
    ExprPtr getInvalidExpr() {
        return mUnhandledExpr;
    }

public:
    std::string visitExpr(const ExprPtr& expr) {
        mUnhandledExpr = expr;
        llvm::errs() << "Unhandled expr " << *expr << "\n";
        return "__UNHANDLED_EXPR__";
    }

    std::string visitLiteral(const ExprRef<LiteralExpr>& expr)
    {
        if (auto intLit = llvm::dyn_cast<IntLiteralExpr>(expr)) {
            auto val = intLit->getValue();
            return val < 0 ? "(" + std::to_string(val) + ")" : std::to_string(intLit->getValue());
        }

        if (auto boolLit = llvm::dyn_cast<BoolLiteralExpr>(expr)) {
            return boolLit->getValue() ? "true" : "false";
        }

        if (auto realLit = llvm::dyn_cast<RealLiteralExpr>(expr)) {
            auto val = realLit->getValue();
            return std::to_string(val.numerator()) + "%" + std::to_string(val.denominator());
        }

        if(auto bvLit = llvm::dyn_cast<BvLiteralExpr>(expr)) {
            auto exactValue = bvLit->getValue().getZExtValue();
            auto exactString = std::bitset<sizeof (exactValue) * 8>(exactValue).to_string();
            return std::to_string(bvLit->getType().getWidth()) + "'b" + exactString.substr(exactString.length() - bvLit->getType().getWidth());
        }

        if(auto arrLit = llvm::dyn_cast<ArrayLiteralExpr>(expr)) {
            std::string arrLitStr = "[";

            const auto& kvPairs = arrLit->getMap();
            if(kvPairs.size() > 0) {
                for(const auto& [index, elem] : kvPairs) {
                    arrLitStr += this->walk(index) + " <- " + this->walk(elem) + ", ";
                }
                arrLitStr += "default <- ";
            }
            else {
                arrLitStr += "<" + ThetaType::getThetaTypeName(arrLit->getType().getIndexType()) + ">default <- ";
            }

            if(arrLit->hasDefault()) {
                arrLitStr += this->walk(arrLit->getDefault());
            }
            else {
                arrLitStr += ThetaType::getThetaTypeDefaultValue(arrLit->getType().getElementType());
            }

            arrLitStr += "]";
            return arrLitStr;
        }

        return visitExpr(expr);
    }

    std::string visitVarRef(const ExprRef<VarRefExpr>& expr) {
        std::string newName = mReplacedNames(&expr->getVariable());
        if (!newName.empty()) {
            return newName;
        }

        return expr->getVariable().getName();
    }

    std::string visitNot(const ExprRef<NotExpr>& expr) {
        return "(not " + getOperand(0) + ")";
    }

    // Binary
    std::string visitAdd(const ExprRef<AddExpr>& expr) {
        if(expr->getType().isBvType()) {
            return "(" + getOperand(0) + " bvadd " + getOperand(1) + ")";
        }
        else {
        return "(" + getOperand(0) + " + " + getOperand(1) + ")";
    }
    }

    std::string visitSub(const ExprRef<SubExpr>& expr) {
        if(expr->getType().isBvType()) {
            return "(" + getOperand(0) + " bvsub " + getOperand(1) + ")";
        }
        else {
        return "(" + getOperand(0) + " - " + getOperand(1) + ")";
    }
    }

    std::string visitMul(const ExprRef<MulExpr>& expr) {
        if(expr->getType().isBvType()) {
            return "(" + getOperand(0) + " bvmul " + getOperand(1) + ")";
        }
        else {
        return "(" + getOperand(0) + " * " + getOperand(1) + ")";
    }
    }

    std::string visitDiv(const ExprRef<DivExpr>& expr) {
        return "(" + getOperand(0) + " / " + getOperand(1) + ")";
    }

    std::string visitBvUDiv([[maybe_unused]] const ExprRef<BvUDivExpr>& expr) {
        return "(" + getOperand(0) + " bvudiv " + getOperand(1) + ")";
    }

    std::string visitBvSDiv([[maybe_unused]] const ExprRef<BvSDivExpr>& expr) {
        return "(" + getOperand(0) + " bvsdiv " + getOperand(1) + ")";
    }

    std::string visitMod(const ExprRef<ModExpr>& expr) {
        if(expr->getType().isBvType()) {
            return "(" + getOperand(0) + " bvsmod " + getOperand(1) + ")";
        }
        else {
        return "(" + getOperand(0) + " mod " + getOperand(1) + ")";
    }
    }

    std::string visitBvURem([[maybe_unused]] const ExprRef<BvURemExpr>& expr) {
        return "(" + getOperand(0) + " bvurem " + getOperand(1) + ")";
    }

    std::string visitBvSRem([[maybe_unused]] const ExprRef<BvSRemExpr>& expr) {
        return "(" + getOperand(0) + " bvsrem " + getOperand(1) + ")";
    }

    std::string visitAnd(const ExprRef<AndExpr>& expr)
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

    std::string visitOr(const ExprRef<OrExpr>& expr)
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

    std::string visitImply(const ExprRef<ImplyExpr>& expr) {
        return "(" + getOperand(0) + " imply " + getOperand(1) + ")";
    }

    std::string visitBvAnd([[maybe_unused]] const ExprRef<BvAndExpr>& expr) {
        return "(" + getOperand(0) + " bvand " + getOperand(1) + ")";
    }

    std::string visitBvOr([[maybe_unused]] const ExprRef<BvOrExpr>& expr) {
        return "(" + getOperand(0) + " bvor " + getOperand(1) + ")";
    }

    std::string visitBvXor([[maybe_unused]] const ExprRef<BvXorExpr>& expr) {
        return "(" + getOperand(0) + " bvxor " + getOperand(1) + ")";
    }

    std::string visitShl([[maybe_unused]] const ExprRef<ShlExpr>& expr) {
        return "(" + getOperand(0) + " bvshl " + getOperand(1) + ")";
    }

    std::string visitAShr([[maybe_unused]] const ExprRef<AShrExpr>& expr) {
        return "(" + getOperand(0) + " bvashr " + getOperand(1) + ")";
    }

    std::string visitBvConcat([[maybe_unused]] const ExprRef<BvConcatExpr>& expr) {
        return "(" + getOperand(0) + " ++ " + getOperand(1) + ")";
    }

    std::string visitExtract(const ExprRef<ExtractExpr>& expr) {
        auto from = expr->getOffset();
        auto until = expr->getOffset() + expr->getWidth();
        return "(" + getOperand(0) + ")[" + std::to_string(until) + ":" + std::to_string(from) + "]";
    }

    std::string visitZExt(const ExprRef<ZExtExpr>& expr) {
        auto width = expr->getExtendedWidth();
        return "(" + getOperand(0) + " bv_zero_extend bv[" + std::to_string(width) + "])";
    }

    std::string visitSExt(const ExprRef<SExtExpr>& expr) {
        auto width = expr->getExtendedWidth();
        return "(" + getOperand(0) + " bv_sign_extend bv[" + std::to_string(width) + "])";
    }

    std::string visitLShr([[maybe_unused]] const ExprRef<LShrExpr>& expr) {
        return "(" + getOperand(0) + " bvlshr " + getOperand(1) + ")";
    }
        return "(" + getOperand(0) + " = " + getOperand(1) + ")";
    }
    
    std::string visitNotEq(const ExprRef<NotEqExpr>& expr) {
        return "(" + getOperand(0) + " /= " + getOperand(1) + ")";
    }

    std::string visitLt(const ExprRef<LtExpr>& expr) {
        return "(" + getOperand(0) + " < " + getOperand(1) + ")";
    }

    std::string visitLtEq(const ExprRef<LtEqExpr>& expr) {
        return "(" + getOperand(0) + " <= " + getOperand(1) + ")";
    }

    std::string visitGt(const ExprRef<GtExpr>& expr) {
        return "(" + getOperand(0) + " > " + getOperand(1) + ")";
    }

    std::string visitGtEq(const ExprRef<GtEqExpr>& expr) {
        return "(" + getOperand(0) + " >= " + getOperand(1) + ")";
    }

    std::string visitBvULt([[maybe_unused]] const ExprRef<BvULtExpr>& expr) {
        return "(" + getOperand(0) + " bvult " + getOperand(1) + ")";
    }

    std::string visitBvULtEq([[maybe_unused]] const ExprRef<BvULtEqExpr>& expr) {
        return "(" + getOperand(0) + " bvule " + getOperand(1) + ")";
    }

    std::string visitBvUGt([[maybe_unused]] const ExprRef<BvUGtExpr>& expr) {
        return "(" + getOperand(0) + " bvugt " + getOperand(1) + ")";
    }

    std::string visitBvUGtEq([[maybe_unused]] const ExprRef<BvUGtEqExpr>& expr) {
        return "(" + getOperand(0) + " bvuge " + getOperand(1) + ")";
    }

    std::string visitBvSLt([[maybe_unused]] const ExprRef<BvSLtExpr>& expr) {
        return "(" + getOperand(0) + " bvslt " + getOperand(1) + ")";
    }

    std::string visitBvSLtEq([[maybe_unused]] const ExprRef<BvSLtEqExpr>& expr) {
        return "(" + getOperand(0) + " bvsle " + getOperand(1) + ")";
    }

    std::string visitBvSGt([[maybe_unused]] const ExprRef<BvSGtExpr>& expr) {
        return "(" + getOperand(0) + " bvsgt " + getOperand(1) + ")";
    }

    std::string visitBvSGtEq([[maybe_unused]] const ExprRef<BvSGtEqExpr>& expr) {
        return "(" + getOperand(0) + " bvsge " + getOperand(1) + ")";
    }
        return "(if " + getOperand(0) + " then " + getOperand(1) + " else " + getOperand(2) + ")";
    }

    std::string visitArrayRead(const ExprRef<ArrayReadExpr>& expr) {
        return  "(" + getOperand(0) + ")[" + getOperand(1) + "]";
    }

    std::string visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr) {
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
    ThetaExprPrinter printer(variableNames);

    return printer.walk(expr);
}