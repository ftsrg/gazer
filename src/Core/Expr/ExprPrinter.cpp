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
#include "gazer/Core/Expr/ExprUtils.h"
#include "gazer/Core/Expr/ExprWalker.h"
#include "gazer/ADT/StringUtils.h"

#include <llvm/ADT/Twine.h>
#include <llvm/Support/raw_ostream.h>

#include <boost/algorithm/string.hpp>
#include <boost/range/irange.hpp>

#include <algorithm>

using namespace gazer;
using llvm::dyn_cast;

// Default printers
//===----------------------------------------------------------------------===//

void NonNullaryExpr::print(llvm::raw_ostream& os) const
{
    size_t i = 0;
    os << getType().getName() << " " << Expr::getKindName(getKind()) << "(";
    while (i < getNumOperands() - 1) {
        getOperand(i)->print(os);
        os << ",";
        ++i;
    }

    getOperand(i)->print(os);
    os << ")";
}

void ExtractExpr::print(llvm::raw_ostream& os) const
{
    os << getType().getName() << " " << Expr::getKindName(getKind()) << "(";
    getOperand()->print(os);
    os << ", " << mOffset << ", " << mWidth << ")";
}

llvm::raw_ostream& gazer::operator<<(llvm::raw_ostream& os, const Expr& expr)
{
    expr.print(os);
    return os;
}

// Infix expression printing
//===----------------------------------------------------------------------===//

namespace
{

using llvm::Twine;

std::string castTypeName(const llvm::Twine& name, const Type& from, const Type& to)
{
    std::string fromStr = from.getName();
    std::string toStr = to.getName();

    boost::to_lower(fromStr);
    boost::to_lower(toStr);

    return (name + "." + Twine(fromStr) + "." + Twine(toStr)).str();
}

class InfixPrintVisitor : public ExprWalker<InfixPrintVisitor, std::string>
{
    friend class ExprWalker<InfixPrintVisitor, std::string>;
public:
    InfixPrintVisitor(unsigned radix)
        : mRadix(radix)
    {
        assert(mRadix == 2 || mRadix == 8 || mRadix == 16 || mRadix == 10);
    }
private:
    std::string printOp(const ExprRef<NonNullaryExpr>& expr, size_t idx)
    {
        std::string opStr = getOperand(idx);
        if (expr->getOperand(idx)->isNullary()) {
            return opStr;
        }

        return (Twine("(") + Twine(opStr) + ")").str();
    }

private:
    std::string visitExpr(const ExprPtr& expr) { llvm_unreachable("Unknown expression kind!"); }
    std::string visitUndef(const ExprRef<UndefExpr>& expr) { return "undef"; }
    std::string visitVarRef(const ExprRef<VarRefExpr>& expr)
    {
        return expr->getVariable().getName();
    }

    std::string visitLiteral(const ExprRef<LiteralExpr>& expr)
    {        
        llvm::SmallVector<char, 32> buffer;

        switch (expr->getType().getTypeID()) {
            case Type::BoolTypeID: {
                auto bl = llvm::cast<BoolLiteralExpr>(expr);
                return bl->getValue() ? "true" : "false";
            }
            case Type::BvTypeID: {
                auto bv = llvm::cast<BvLiteralExpr>(expr);
                bv->getValue().toStringSigned(buffer, mRadix);

                return (
                    getRadixPrefix() + Twine{buffer}
                    + "bv" + Twine{bv->getType().getWidth()}
                ).str();
            }
            case Type::FloatTypeID: {
                auto fl = llvm::cast<FloatLiteralExpr>(expr);
                fl->getValue().toString(buffer);

                return (Twine{buffer} + "fp" + Twine{fl->getType().getWidth()}).str();
            }
            case Type::IntTypeID: {
                auto il = llvm::cast<IntLiteralExpr>(expr);
                return std::to_string(il->getValue());
            }
            case Type::RealTypeID: {
                auto rl = llvm::cast<RealLiteralExpr>(expr);

                return (Twine(rl->getValue().numerator()) + "%" + Twine(rl->getValue().denominator())).str();
            }
            // TODO: Tuples, ArrayType
        }

        llvm_unreachable("Unknown literal expression kind.");
    }

    // Define some helper macros
    #define PRINT_UNARY_PREFIX(NAME, OPERATOR)                                  \
    std::string visit##NAME(const ExprRef<NAME##Expr>& expr)                    \
    {                                                                           \
        return (Twine{OPERATOR} + "(" + getOperand(0) + ")").str();             \
    }

    #define PRINT_UNARY_CAST(NAME, OPERATOR)                                    \
    std::string visit##NAME(const ExprRef<NAME##Expr>& expr)                    \
    {                                                                           \
        auto fname = castTypeName(                                              \
            OPERATOR, expr->getOperand()->getType(), expr->getType());          \
        return fname + "(" + getOperand(0) + ")";                               \
    }

    #define PRINT_BINARY_INFIX(NAME, OPERATOR)                                  \
    std::string visit##NAME(const ExprRef<NAME##Expr>& expr)                    \
    {                                                                           \
        return (getOperand(0)) + (OPERATOR) + (getOperand(1));                  \
    }

    #define PRINT_BINARY_PREFIX(NAME, OPERATOR)                                 \
    std::string visit##NAME(const ExprRef<NAME##Expr>& expr)                    \
    {                                                                           \
        return (Twine{OPERATOR} + "("                                           \
            + (getOperand(0)) + "," + (getOperand(1)) + ")"                     \
        ).str();                                                                \
    }

    // Unary
    std::string visitNot(const ExprRef<NotExpr>& expr)
    {
        return "not " + printOp(expr, 0);
    }

    PRINT_UNARY_CAST(ZExt, "zext")
    PRINT_UNARY_CAST(SExt, "sext")

    std::string visitExtract(const ExprRef<ExtractExpr>& expr)
    {
        auto fname = castTypeName("extract", expr->getOperand()->getType(), expr->getType());
        return (fname + "(" + getOperand(0)
                     + ", " + Twine(expr->getOffset())
                     + ", " + Twine(expr->getWidth()) + ")").str();
    }

    PRINT_BINARY_INFIX(Add,     " + ")
    PRINT_BINARY_INFIX(Sub,     " - ")
    PRINT_BINARY_INFIX(Mul,     " * ")
    PRINT_BINARY_INFIX(Div,     " / ")
    PRINT_BINARY_INFIX(BvSDiv,  " sdiv ")
    PRINT_BINARY_INFIX(BvUDiv,  " udiv ")
    PRINT_BINARY_INFIX(BvSRem,  " srem ")
    PRINT_BINARY_INFIX(BvURem,  " urem ")

    PRINT_BINARY_PREFIX(Shl,    "bv.shl")
    PRINT_BINARY_PREFIX(LShr,   "bv.lhsr")
    PRINT_BINARY_PREFIX(AShr,   "bv.ashr")
    PRINT_BINARY_PREFIX(BvAnd,  "bv.and")
    PRINT_BINARY_PREFIX(BvOr,   "bv.or")
    PRINT_BINARY_PREFIX(BvXor,  "bv.xor")

    // Logic
    PRINT_BINARY_INFIX(Imply,   "imply")

    std::string visitAnd(const ExprRef<AndExpr>& expr)
    {
        std::string buffer;
        llvm::raw_string_ostream rso{buffer};

        auto r = boost::irange<size_t>(0, expr->getNumOperands());

        join_print_as(rso, r.begin(), r.end(), " and ", [this, &expr](auto& rso, size_t i) {
            rso << this->printOp(expr, i);
        });

        return rso.str();
    }

    std::string visitOr(const ExprRef<OrExpr>& expr)
    {
        std::string buffer;
        llvm::raw_string_ostream rso{buffer};

        auto r = boost::irange<size_t>(0, expr->getNumOperands());

        join_print_as(rso, r.begin(), r.end(), " or ", [this, &expr](auto& rso, size_t i) {
            rso << this->printOp(expr, i);
        });

        return rso.str();
    }

    // Compare
    PRINT_BINARY_INFIX(Eq,            " = ");
    PRINT_BINARY_INFIX(NotEq,         " <> ");
    PRINT_BINARY_INFIX(Lt,            " < ");
    PRINT_BINARY_INFIX(LtEq,          " <= ");
    PRINT_BINARY_INFIX(Gt,            " > ");
    PRINT_BINARY_INFIX(GtEq,          " >= ");

    PRINT_BINARY_PREFIX(BvSLt,        "slt")
    PRINT_BINARY_PREFIX(BvSLtEq,      "sle")
    PRINT_BINARY_PREFIX(BvSGt,        "sgt")
    PRINT_BINARY_PREFIX(BvSGtEq,      "sge")
    PRINT_BINARY_PREFIX(BvULt,        "ult")
    PRINT_BINARY_PREFIX(BvULtEq,      "ule")
    PRINT_BINARY_PREFIX(BvUGt,        "ugt")
    PRINT_BINARY_PREFIX(BvUGtEq,      "uge")

    // Floating-point queries
    PRINT_UNARY_PREFIX(FIsNan, "fp.is_nan")
    PRINT_UNARY_PREFIX(FIsInf, "fp.is_inf")

    // Floating-point casts
    PRINT_UNARY_CAST(FCast,         "fcast")
    PRINT_UNARY_CAST(SignedToFp,    "si_to_fp")
    PRINT_UNARY_CAST(UnsignedToFp,  "ui_to_fp")
    PRINT_UNARY_CAST(FpToSigned,    "fp_to_si")
    PRINT_UNARY_CAST(FpToUnsigned,  "fp_to_ui")

    // Floating-point arithmetic
    PRINT_BINARY_PREFIX(FAdd,   "fp.add")
    PRINT_BINARY_PREFIX(FSub,   "fp.sub")
    PRINT_BINARY_PREFIX(FMul,   "fp.mul")
    PRINT_BINARY_PREFIX(FDiv,   "fp.div")

    // Floating-point compare
    PRINT_BINARY_PREFIX(FEq,   "fp.eq")
    PRINT_BINARY_PREFIX(FGt,   "fp.gt")
    PRINT_BINARY_PREFIX(FGtEq, "fp.ge")
    PRINT_BINARY_PREFIX(FLt,   "fp.lt")
    PRINT_BINARY_PREFIX(FLtEq, "fp.le")

    // Ternary
    std::string visitSelect(const ExprRef<SelectExpr>& expr)
    {
        return (Twine("if ") + printOp(expr, 0)
            + Twine(" then ") + printOp(expr, 1)
            + Twine(" else ") + printOp(expr, 2)).str();
    }

    // Arrays
    std::string visitArrayRead(const ExprRef<ArrayReadExpr>& expr) 
    {
        return this->visitNonNullary(expr);
    }

    std::string visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr)
    {
        return this->visitNonNullary(expr);
    }

    llvm::StringRef getRadixPrefix() const
    {
        switch (mRadix) {
            case 2:  return "2#";
            case 8:  return "8#";
            case 10: return "";
            case 16: return "16#";
        }

        llvm_unreachable("Radix may only be 2, 8, 10, or 16");
    }

private:
    unsigned mRadix;
};

} // end anonymous namespace

namespace gazer
{

void FormatPrintExpr(const ExprPtr& expr, llvm::raw_ostream& os)
{
    // TODO
    InfixPrintVisitor visitor{10};
    os << visitor.walk(expr);
}

void InfixPrintExpr(const ExprPtr& expr, llvm::raw_ostream& os, unsigned bvRadix)
{
    InfixPrintVisitor visitor{bvRadix};
    os << visitor.walk(expr);
}

} // end namespace gazer