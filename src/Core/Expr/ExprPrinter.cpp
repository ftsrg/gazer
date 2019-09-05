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

namespace
{

using llvm::Twine;

std::string castTypeName(llvm::Twine name, const Type& from, const Type& to)
{
    std::string fromStr = from.getName();
    std::string toStr = to.getName();

    boost::to_lower(fromStr);
    boost::to_lower(toStr);

    return (name + "." + Twine(fromStr) + "." + Twine(toStr)).str();
}

class InfixPrintVisitor : public ExprWalker<InfixPrintVisitor, std::string>
{
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

public:
    std::string visitExpr(const ExprPtr& expr) { return "???"; }    
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
                bv->getValue().toStringUnsigned(buffer, mRadix);

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
        }

        llvm_unreachable("Unknown literal expression kind.");
    }

    // Unary
    std::string visitNot(const ExprRef<NotExpr>& expr)
    {
        return "not " + printOp(expr, 0);
    }

    std::string visitZExt(const ExprRef<ZExtExpr>& expr)
    {
        auto fname = castTypeName("zext", expr->getOperand()->getType(), expr->getType());
        return fname + "(" + getOperand(0) + ")";
    }

    std::string visitSExt(const ExprRef<SExtExpr>& expr)
    {
        auto fname = castTypeName("sext", expr->getOperand()->getType(), expr->getType());
        return fname + "(" + getOperand(0) + ")";
    }

    std::string visitExtract(const ExprRef<ExtractExpr>& expr)
    {
        auto fname = castTypeName("extract", expr->getOperand()->getType(), expr->getType());
        return (fname + "(" + getOperand(0)
                     + ", " + Twine(expr->getOffset())
                     + ", " + Twine(expr->getWidth()) + ")").str();
    }

    // Binary

    #define PRINT_BINARY_INFIX(NAME, OPERATOR)                                  \
    std::string visit##NAME(const ExprRef<NAME##Expr>& expr)                    \
    {                                                                           \
        return (getOperand(0)) + (OPERATOR) + (getOperand(1));                  \
    }

    PRINT_BINARY_INFIX(Add,     " + ")
    PRINT_BINARY_INFIX(Sub,     " - ")
    PRINT_BINARY_INFIX(Mul,     " * ")
    PRINT_BINARY_INFIX(Div,     " / ")
    PRINT_BINARY_INFIX(BvSDiv,  " sdiv ")
    PRINT_BINARY_INFIX(BvUDiv,  " udiv ")
    PRINT_BINARY_INFIX(BvSRem,  " srem ")
    PRINT_BINARY_INFIX(BvURem,  " urem ")

    #define PRINT_BINARY_PREFIX(NAME, OPERATOR)                                 \
    std::string visit##NAME(const ExprRef<NAME##Expr>& expr)                    \
        {                                                                       \
        return (Twine{OPERATOR} + "("                                           \
            + (getOperand(0)) + "," + (getOperand(1)) + ")"                     \
        ).str();                                                                \
    }

    PRINT_BINARY_PREFIX(Shl,    "bv.shl")
    PRINT_BINARY_PREFIX(LShr,   "bv.lhsr")
    PRINT_BINARY_PREFIX(AShr,   "bv.ashr")
    PRINT_BINARY_PREFIX(BvAnd,  "bv.and")
    PRINT_BINARY_PREFIX(BvOr,   "bv.or")
    PRINT_BINARY_PREFIX(BvXor,  "bv.xor")

    // Logic
    PRINT_BINARY_INFIX(Xor,     "xor")
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
    PRINT_BINARY_INFIX(Eq, "=");
    PRINT_BINARY_INFIX(NotEq, "<>");

    PRINT_BINARY_PREFIX(BvSLt,        "slt")
    PRINT_BINARY_PREFIX(BvSLtEq,      "sle")
    PRINT_BINARY_PREFIX(BvSGt,        "sgt")
    PRINT_BINARY_PREFIX(BvSGtEq,      "sge")
    PRINT_BINARY_PREFIX(BvULt,        "ult")
    PRINT_BINARY_PREFIX(BvULtEq,      "ule")
    PRINT_BINARY_PREFIX(BvUGt,        "ugt")
    PRINT_BINARY_PREFIX(BvUGtEq,      "uge")

    // Floating-point queries
    std::string visitFIsNan(const ExprRef<FIsNanExpr>& expr)
    {
        return "fp.is_nan(" + getOperand(0) + ")";
    }

    std::string visitFIsInf(const ExprRef<FIsInfExpr>& expr)
    {
        return "fp.is_inf(" + getOperand(0) + ")";
    }


    // Floating-point casts
    std::string visitFCast(const ExprRef<FCastExpr>& expr)
    {
        return castTypeName("fcast", expr->getType(), expr->getOperand()->getType()) + "(" + getOperand(0) + ")";
    }

    std::string visitSignedToFp(const ExprRef<SignedToFpExpr>& expr)
    {
        return castTypeName("si_to_fp", expr->getType(), expr->getOperand()->getType()) + "(" + getOperand(0) + ")";
    }

    std::string visitUnsignedToFp(const ExprRef<UnsignedToFpExpr>& expr)
    {
        return castTypeName("ui_to_fp", expr->getType(), expr->getOperand()->getType()) + "(" + getOperand(0) + ")";
    }

    std::string visitFpToSigned(const ExprRef<FpToSignedExpr>& expr)
    {
        return castTypeName("fp_to_si", expr->getType(), expr->getOperand()->getType()) + "(" + getOperand(0) + ")";
    }

    std::string visitFpToUnsigned(const ExprRef<FpToUnsignedExpr>& expr)
    {
        return castTypeName("fp_to_ui", expr->getType(), expr->getOperand()->getType()) + "(" + getOperand(0) + ")";
    }

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