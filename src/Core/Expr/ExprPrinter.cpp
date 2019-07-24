#include "gazer/Core/Expr/ExprUtils.h"
#include "gazer/Core/ExprVisitor.h"

#include <llvm/Support/raw_ostream.h>

#include <algorithm>

using namespace gazer;

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

}

namespace gazer
{

void FormatPrintExpr(const ExprPtr& expr, llvm::raw_ostream& os)
{
    FormattedPrintVisitor visitor(os);
    visitor.visit(expr);
}

}