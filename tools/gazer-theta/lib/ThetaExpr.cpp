#include "ThetaCfaGenerator.h"
#include "gazer/Core/Expr/ExprWalker.h"
#include "gazer/ADT/StringUtils.h"

#include <llvm/ADT/DenseMap.h>

#include <boost/range/irange.hpp>

using namespace gazer;

class ThetaExprPrinter : public ExprWalker<ThetaExprPrinter, std::string>
{
public:
    ThetaExprPrinter(const llvm::DenseMap<Variable*, std::string>& replacedNames)
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
        return "???";
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

        return visitExpr(expr);
    }

    std::string visitVarRef(const ExprRef<VarRefExpr>& expr) {
        std::string newName = mReplacedNames.lookup(&expr->getVariable());
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
        return "(" + getOperand(0) + " + " + getOperand(1) + ")";
    }

    std::string visitSub(const ExprRef<SubExpr>& expr) {
        return "(" + getOperand(0) + " - " + getOperand(1) + ")";
    }

    std::string visitMul(const ExprRef<MulExpr>& expr) {
        return "(" + getOperand(0) + " * " + getOperand(1) + ")";
    }

    std::string visitDiv(const ExprRef<DivExpr>& expr) {
        return "(" + getOperand(0) + " / " + getOperand(1) + ")";
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

    std::string visitXor(const ExprRef<XorExpr>& expr) {
        return "(" + getOperand(0) + " /= " + getOperand(1) + ")";
    }

    std::string visitImply(const ExprRef<ImplyExpr>& expr) {
        return "(" + getOperand(0) + " imply " + getOperand(1) + ")";
    }

    std::string visitEq(const ExprRef<EqExpr>& expr) {
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

    std::string visitSelect(const ExprRef<SelectExpr>& expr) {
        return "(if " + getOperand(0) + " then " + getOperand(1) + " else " + getOperand(2) + ")";
    }

private:
    ExprPtr mUnhandledExpr = nullptr;
    const llvm::DenseMap<Variable*, std::string>& mReplacedNames;
};

std::string gazer::theta::printThetaExpr(const ExprPtr& expr)
{
    llvm::DenseMap<Variable*, std::string> names;
    ThetaExprPrinter printer(names);

    return printer.walk(expr);
}