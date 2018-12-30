#include "gazer/Core/Expr/ExprEvaluator.h"

using namespace gazer;
using llvm::cast;
using llvm::dyn_cast;

/// Checks for undefs among operands.

std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitUndef(const std::shared_ptr<UndefExpr>& expr) {
    assert(!"Invalid undef expression");
}

std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitExpr(const ExprPtr& expr)
{
    assert(!"Unhandled expression type in ExprEvaluatorBase!");
}

std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitLiteral(const std::shared_ptr<LiteralExpr>& expr) {
    return expr;
}

std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitVarRef(const std::shared_ptr<VarRefExpr>& expr) {
    return this->getVariableValue(expr->getVariable());
}

// Unary
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitNot(const std::shared_ptr<NotExpr>& expr)
{
    auto boolLit = dyn_cast<BoolLiteralExpr>(visit(expr->getOperand()).get());
    return BoolLiteralExpr::Get(!boolLit->getValue());
}

std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitZExt(const std::shared_ptr<ZExtExpr>& expr)
{
    auto bvLit = dyn_cast<BvLiteralExpr>(visit(expr->getOperand()).get());

    return BvLiteralExpr::Get(bvLit->getValue().zext(expr->getExtendedWidth()));
}

std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitSExt(const std::shared_ptr<SExtExpr>& expr)
{
    auto bvLit = dyn_cast<BvLiteralExpr>(visit(expr->getOperand()).get());

    return BvLiteralExpr::Get(bvLit->getValue().sext(expr->getExtendedWidth()));
}

std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitExtract(const std::shared_ptr<ExtractExpr>& expr)
{
    auto bvLit = dyn_cast<BvLiteralExpr>(visit(expr->getOperand()).get());

    return BvLiteralExpr::Get(bvLit->getValue().extractBits(expr->getExtractedWidth(), expr->getOffset()));
}

template<Expr::ExprKind Kind>
static std::shared_ptr<LiteralExpr> EvalBinaryArithmetic(
    ExprEvaluatorBase* visitor,
    const std::shared_ptr<ArithmeticExpr<Kind>>& expr)
{
    static_assert(Expr::FirstBinaryArithmetic <= Kind && Kind <= Expr::LastBinaryArithmetic,
        "An arithmetic expression must have an arithmetic expression kind.");

    auto left = dyn_cast<BvLiteralExpr>(visitor->visit(expr->getLeft()).get())->getValue();
    auto right = dyn_cast<BvLiteralExpr>(visitor->visit(expr->getRight()).get())->getValue();

    // TODO: Add support for Int types as well...

    switch (Kind) {
        case Expr::Add: return BvLiteralExpr::Get(left + right);
        case Expr::Sub: return BvLiteralExpr::Get(left - right);
        case Expr::Mul: return BvLiteralExpr::Get(left * right);
        case Expr::SDiv: return BvLiteralExpr::Get(left.sdiv(right));
        case Expr::UDiv: return BvLiteralExpr::Get(left.udiv(right));
        case Expr::SRem: return BvLiteralExpr::Get(left.srem(right));
        case Expr::URem: return BvLiteralExpr::Get(left.urem(right));
        case Expr::Shl: return BvLiteralExpr::Get(left.shl(right));
        case Expr::LShr: return BvLiteralExpr::Get(left.lshr(right.getLimitedValue()));
        case Expr::AShr: return BvLiteralExpr::Get(left.ashr(right.getLimitedValue()));
        case Expr::BAnd: return BvLiteralExpr::Get(left & right);
        case Expr::BOr: return BvLiteralExpr::Get(left | right);
        case Expr::BXor: return BvLiteralExpr::Get(left ^ right);
    }
    
    llvm_unreachable("Unknown binary arithmetic expression kind.");
}

// Binary
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitAdd(const std::shared_ptr<AddExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitSub(const std::shared_ptr<SubExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitMul(const std::shared_ptr<MulExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitSDiv(const std::shared_ptr<SDivExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitUDiv(const std::shared_ptr<UDivExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitSRem(const std::shared_ptr<SRemExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitURem(const std::shared_ptr<URemExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitShl(const std::shared_ptr<ShlExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitLShr(const std::shared_ptr<LShrExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitAShr(const std::shared_ptr<AShrExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitBAnd(const std::shared_ptr<BAndExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitBOr(const std::shared_ptr<BOrExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitBXor(const std::shared_ptr<BXorExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}

// Logic
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitAnd(const std::shared_ptr<AndExpr>& expr) {
    return this->visitNonNullary(expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitOr(const std::shared_ptr<OrExpr>& expr) {
    return this->visitNonNullary(expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitXor(const std::shared_ptr<XorExpr>& expr) {
    return this->visitNonNullary(expr);
}

template<Expr::ExprKind Kind>
static std::shared_ptr<LiteralExpr> EvalCompareExpr(
    ExprEvaluatorBase* visitor, 
    const std::shared_ptr<CompareExpr<Kind>>& expr)
{
    static_assert(Expr::FirstCompare <= Kind && Kind <= Expr::LastCompare,
        "A compare expression must have a compare expression kind.");

    auto left = dyn_cast<BvLiteralExpr>(visitor->visit(expr->getLeft()).get())->getValue();
    auto right = dyn_cast<BvLiteralExpr>(visitor->visit(expr->getRight()).get())->getValue();

    switch (Kind) {
        case Expr::Eq: return BoolLiteralExpr::Get(left.eq(right));
        case Expr::NotEq: return BoolLiteralExpr::Get(left.ne(right));
        case Expr::SLt: return BoolLiteralExpr::Get(left.slt(right));
        case Expr::SLtEq: return BoolLiteralExpr::Get(left.sle(right));
        case Expr::SGt: return BoolLiteralExpr::Get(left.sgt(right));
        case Expr::SGtEq: return BoolLiteralExpr::Get(left.sge(right));
        case Expr::ULt: return BoolLiteralExpr::Get(left.ult(right));
        case Expr::ULtEq: return BoolLiteralExpr::Get(left.ule(right));
        case Expr::UGt: return BoolLiteralExpr::Get(left.ugt(right));
        case Expr::UGtEq: return BoolLiteralExpr::Get(left.uge(right));
    }

    llvm_unreachable("Unknown binary arithmetic expression kind.");
}

// Compare
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitEq(const std::shared_ptr<EqExpr>& expr) {
    return EvalCompareExpr(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitNotEq(const std::shared_ptr<NotEqExpr>& expr) {
    return EvalCompareExpr(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitSLt(const std::shared_ptr<SLtExpr>& expr) {
    return EvalCompareExpr(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitSLtEq(const std::shared_ptr<SLtEqExpr>& expr) {
    return EvalCompareExpr(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitSGt(const std::shared_ptr<SGtExpr>& expr) {
    return EvalCompareExpr(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitSGtEq(const std::shared_ptr<SGtEqExpr>& expr) {
    return EvalCompareExpr(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitULt(const std::shared_ptr<ULtExpr>& expr) {
    return EvalCompareExpr(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitULtEq(const std::shared_ptr<ULtEqExpr>& expr) {
    return EvalCompareExpr(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitUGt(const std::shared_ptr<UGtExpr>& expr) {
    return EvalCompareExpr(this, expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitUGtEq(const std::shared_ptr<UGtEqExpr>& expr) {
    return EvalCompareExpr(this, expr);
}

// Floating-point queries
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitFIsNan(const std::shared_ptr<FIsNanExpr>& expr) {
    return this->visitNonNullary(expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitFIsInf(const std::shared_ptr<FIsInfExpr>& expr) {
    return this->visitNonNullary(expr);
}

// Floating-point arithmetic
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitFAdd(const std::shared_ptr<FAddExpr>& expr) {
    return this->visitNonNullary(expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitFSub(const std::shared_ptr<FSubExpr>& expr) {
    return this->visitNonNullary(expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitFMul(const std::shared_ptr<FMulExpr>& expr) {
    return this->visitNonNullary(expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitFDiv(const std::shared_ptr<FDivExpr>& expr) {
    return this->visitNonNullary(expr);
}

// Floating-point compare
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitFEq(const std::shared_ptr<FEqExpr>& expr) {
    return this->visitNonNullary(expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitFGt(const std::shared_ptr<FGtExpr>& expr) {
    return this->visitNonNullary(expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitFGtEq(const std::shared_ptr<FGtEqExpr>& expr) {
    return this->visitNonNullary(expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitFLt(const std::shared_ptr<FLtExpr>& expr) {
    return this->visitNonNullary(expr);
}
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitFLtEq(const std::shared_ptr<FLtEqExpr>& expr) {
    return this->visitNonNullary(expr);
}

// Ternary
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitSelect(const std::shared_ptr<SelectExpr>& expr) {
    // TODO: Support Int and Float...
    auto cond = dyn_cast<BoolLiteralExpr>(visit(expr->getCondition()).get());
    auto then = dyn_cast<BvLiteralExpr>(visit(expr->getThen()).get());
    auto elze = dyn_cast<BvLiteralExpr>(visit(expr->getElse()).get());

    return BvLiteralExpr::Get(cond->getValue() ? then->getValue() : elze->getValue());
}

// Arrays
std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitArrayRead(const std::shared_ptr<ArrayReadExpr>& expr) {
    return this->visitNonNullary(expr);
}

std::shared_ptr<LiteralExpr> ExprEvaluatorBase::visitArrayWrite(const std::shared_ptr<ArrayWriteExpr>& expr) {
    return this->visitNonNullary(expr);
}