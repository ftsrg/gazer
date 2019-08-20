#include "gazer/Core/Expr/ExprUtils.h"
#include "gazer/Core/ExprVisitor.h"

#include <llvm/ADT/Twine.h>
#include <llvm/Support/raw_ostream.h>

#include <boost/algorithm/string.hpp>

#include <algorithm>

using namespace gazer;
using llvm::dyn_cast;

namespace
{

class FormattedPrintVisitor : public ExprVisitor<void>
{
public:
    FormattedPrintVisitor(llvm::raw_ostream& os)
        : mOS(os), mIndent(0)
    {}

protected:
    void visitExpr(const ExprPtr& expr) override {
        this->indent();
        expr->print(mOS);
    }

    void visitNonNullary(const ExprRef<NonNullaryExpr>& expr) override {
        this->indent();
        if (std::all_of(expr->op_begin(), expr->op_end(), [](const ExprPtr& op) {
            return op->isNullary();
        })) {
            expr->print(mOS);
            return;
        }

        mOS << Expr::getKindName(expr->getKind()) << "(\n";

        size_t numOps = expr->getNumOperands();
        size_t i = 0;
        mIndent++;
        while (i < numOps - 1) {
            this->visit(expr->getOperand(i));
            mOS << ",\n";
            i++;
        }
        this->visit(expr->getOperand(i));
        mOS << "\n";
        mIndent--;
        this->indent();
        mOS << ")";
    }

    void visitExtract(const ExprRef<ExtractExpr>& expr) override {
        this->indent();
        expr->print(mOS);
    }

private:
    void indent() {
        for (size_t i = 0; i < mIndent; ++i) {
            mOS << "  ";
        }
    }

private:
    llvm::raw_ostream& mOS;
    size_t mIndent;
};

using llvm::Twine;

std::string castTypeName(llvm::Twine name, const Type& from, const Type& to)
{
    std::string fromStr = from.getName();
    std::string toStr = to.getName();

    boost::to_lower(fromStr);
    boost::to_lower(toStr);

    return (name + "." + Twine(fromStr) + "." + Twine(toStr)).str();
}

class InfixPrintVisitor : public ExprVisitor<std::string>
{
public:
    InfixPrintVisitor(unsigned radix)
        : mRadix(radix)
    {
        assert(mRadix == 2 || mRadix == 8 || mRadix == 16 || mRadix == 10);
    }

    std::string visitExpr(const ExprPtr& expr) override { return "???"; }    
    std::string visitUndef(const ExprRef<UndefExpr>& expr) override { return "undef"; }
    std::string visitVarRef(const ExprRef<VarRefExpr>& expr) override {
        return expr->getVariable().getName();
    }

    std::string visitLiteral(const ExprRef<LiteralExpr>& expr) override
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
        }

        llvm_unreachable("Unknown literal expression kind.");
    }

    // Unary
    std::string visitNot(const ExprRef<NotExpr>& expr) override
    {
        return "not " + visit(expr->getOperand());
    }

    std::string visitZExt(const ExprRef<ZExtExpr>& expr) override
    {
        auto fname = castTypeName("zext", expr->getOperand()->getType(), expr->getType());
        return fname + "(" + visit(expr->getOperand()) + ")";
    }

    std::string visitSExt(const ExprRef<SExtExpr>& expr) override
    {
        auto fname = castTypeName("sext", expr->getOperand()->getType(), expr->getType());
        return fname + "(" + visit(expr->getOperand()) + ")";
    }

    std::string visitExtract(const ExprRef<ExtractExpr>& expr) override
    {
        auto fname = castTypeName("extract", expr->getOperand()->getType(), expr->getType());
        return (fname + "(" + visit(expr->getOperand())
                     + ", " + Twine(expr->getOffset())
                     + ", " + Twine(expr->getWidth()) + ")").str();
    }

    // Binary

    #define PRINT_BINARY_INFIX(NAME, OPERATOR)                              \
    std::string visit##NAME(const ExprRef<NAME##Expr>& expr) override {     \
        return visit(expr->getLeft()) + OPERATOR + visit(expr->getRight()); \
    }

    PRINT_BINARY_INFIX(Add, "+")
    PRINT_BINARY_INFIX(Sub, "-")
    PRINT_BINARY_INFIX(Mul, "*")
    PRINT_BINARY_INFIX(Div, "/")
    PRINT_BINARY_INFIX(BvSDiv, "sdiv")
    PRINT_BINARY_INFIX(BvUDiv, "udiv")
    PRINT_BINARY_INFIX(BvSRem, "srem")
    PRINT_BINARY_INFIX(BvURem, "urem")

    std::string visitShl(const ExprRef<ShlExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitLShr(const ExprRef<LShrExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitAShr(const ExprRef<AShrExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitBvAnd(const ExprRef<BvAndExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitBvOr(const ExprRef<BvOrExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitBvXor(const ExprRef<BvXorExpr>& expr) override {
        return this->visitNonNullary(expr);
    }

    // Logic
    std::string visitAnd(const ExprRef<AndExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitOr(const ExprRef<OrExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitXor(const ExprRef<XorExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitImply(const ExprRef<ImplyExpr>& expr) override {
        return this->visitNonNullary(expr);
    }

    // Compare
    std::string visitEq(const ExprRef<EqExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitNotEq(const ExprRef<NotEqExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    
    std::string visitSLt(const ExprRef<SLtExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitSLtEq(const ExprRef<SLtEqExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitSGt(const ExprRef<SGtExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitSGtEq(const ExprRef<SGtEqExpr>& expr) override {
        return this->visitNonNullary(expr);
    }

    std::string visitULt(const ExprRef<ULtExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitULtEq(const ExprRef<ULtEqExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitUGt(const ExprRef<UGtExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitUGtEq(const ExprRef<UGtEqExpr>& expr) override {
        return this->visitNonNullary(expr);
    }

    // Floating-point queries
    std::string visitFIsNan(const ExprRef<FIsNanExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitFIsInf(const ExprRef<FIsInfExpr>& expr) override {
        return this->visitNonNullary(expr);
    }

    // Floating-point casts
    std::string visitFCast(const ExprRef<FCastExpr>& expr) override {
        return this->visitNonNullary(expr);
    }

    std::string visitSignedToFp(const ExprRef<SignedToFpExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitUnsignedToFp(const ExprRef<UnsignedToFpExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitFpToSigned(const ExprRef<FpToSignedExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitFpToUnsigned(const ExprRef<FpToUnsignedExpr>& expr) override {
        return this->visitNonNullary(expr);
    }

    // Floating-point arithmetic
    std::string visitFAdd(const ExprRef<FAddExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitFSub(const ExprRef<FSubExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitFMul(const ExprRef<FMulExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitFDiv(const ExprRef<FDivExpr>& expr) override {
        return this->visitNonNullary(expr);
    }

    // Floating-point compare
    std::string visitFEq(const ExprRef<FEqExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitFGt(const ExprRef<FGtExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitFGtEq(const ExprRef<FGtEqExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitFLt(const ExprRef<FLtExpr>& expr) override {
        return this->visitNonNullary(expr);
    }
    std::string visitFLtEq(const ExprRef<FLtEqExpr>& expr) override {
        return this->visitNonNullary(expr);
    }

    // Ternary
    std::string visitSelect(const ExprRef<SelectExpr>& expr) override {
        return this->visitNonNullary(expr);
    }

    // Arrays
    std::string visitArrayRead(const ExprRef<ArrayReadExpr>& expr) override {
        return this->visitNonNullary(expr);
    }

    std::string visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr) override {
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

}

namespace gazer
{

void FormatPrintExpr(const ExprPtr& expr, llvm::raw_ostream& os)
{
    FormattedPrintVisitor visitor(os);
    visitor.visit(expr);
}

void InfixPrintExpr(const ExprPtr& expr, llvm::raw_ostream& os, unsigned bvRadix)
{
    InfixPrintVisitor visitor{bvRadix};
    os << visitor.visit(expr);
}

}