#include "gazer/Core/Expr/ExprEvaluator.h"

#include <numeric>

using namespace gazer;
using llvm::cast;
using llvm::dyn_cast;

/// Checks for undefs among operands.

ExprRef<LiteralExpr> ExprEvaluatorBase::visitUndef(const ExprRef<UndefExpr>& expr) {
    assert(!"Invalid undef expression");
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitExpr(const ExprPtr& expr)
{
    assert(!"Unhandled expression type in ExprEvaluatorBase!");
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitLiteral(const ExprRef<LiteralExpr>& expr) {
    return expr;
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitVarRef(const ExprRef<VarRefExpr>& expr) {
    return this->getVariableValue(expr->getVariable());
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitZExt(const ExprRef<ZExtExpr>& expr)
{
    auto bvLit = dyn_cast<BvLiteralExpr>(visit(expr->getOperand()).get());
    auto& type = llvm::cast<BvType>(expr->getType());

    return BvLiteralExpr::Get(type, bvLit->getValue().zext(expr->getExtendedWidth()));
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitSExt(const ExprRef<SExtExpr>& expr)
{
    auto bvLit = dyn_cast<BvLiteralExpr>(visit(expr->getOperand()).get());
    auto& type = llvm::cast<BvType>(expr->getType());

    return BvLiteralExpr::Get(type, bvLit->getValue().sext(expr->getExtendedWidth()));
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitExtract(const ExprRef<ExtractExpr>& expr)
{
    auto bvLit = dyn_cast<BvLiteralExpr>(visit(expr->getOperand()).get());

    return BvLiteralExpr::Get(
        cast<BvType>(expr->getType()),
        bvLit->getValue().extractBits(expr->getExtractedWidth(), expr->getOffset())
    );
}

template<Expr::ExprKind Kind>
static ExprRef<LiteralExpr> EvalBinaryArithmetic(
    ExprEvaluatorBase* visitor,
    const ExprRef<ArithmeticExpr<Kind>>& expr)
{
    static_assert(Expr::FirstBinaryArithmetic <= Kind && Kind <= Expr::LastBinaryArithmetic,
        "An arithmetic expression must have an arithmetic expression kind.");

    auto left = dyn_cast<BvLiteralExpr>(visitor->visit(expr->getLeft()).get())->getValue();
    auto right = dyn_cast<BvLiteralExpr>(visitor->visit(expr->getRight()).get())->getValue();

    // TODO: Add support for Int types as well...

    auto& type = llvm::cast<BvType>(expr->getType());

    switch (Kind) {
        case Expr::Add: return BvLiteralExpr::Get(type, left + right);
        case Expr::Sub: return BvLiteralExpr::Get(type, left - right);
        case Expr::Mul: return BvLiteralExpr::Get(type, left * right);
        case Expr::BvSDiv: return BvLiteralExpr::Get(type, left.sdiv(right));
        case Expr::BvUDiv: return BvLiteralExpr::Get(type, left.udiv(right));
        case Expr::BvSRem: return BvLiteralExpr::Get(type, left.srem(right));
        case Expr::BvURem: return BvLiteralExpr::Get(type, left.urem(right));
        case Expr::Shl: return BvLiteralExpr::Get(type, left.shl(right));
        case Expr::LShr: return BvLiteralExpr::Get(type, left.lshr(right.getLimitedValue()));
        case Expr::AShr: return BvLiteralExpr::Get(type, left.ashr(right.getLimitedValue()));
        case Expr::BvAnd: return BvLiteralExpr::Get(type, left & right);
        case Expr::BvOr: return BvLiteralExpr::Get(type, left | right);
        case Expr::BvXor: return BvLiteralExpr::Get(type, left ^ right);
    }
    
    llvm_unreachable("Unknown binary arithmetic expression kind.");
}

// Binary
ExprRef<LiteralExpr> ExprEvaluatorBase::visitAdd(const ExprRef<AddExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitSub(const ExprRef<SubExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitMul(const ExprRef<MulExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitBvSDiv(const ExprRef<BvSDivExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitBvUDiv(const ExprRef<BvUDivExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitBvSRem(const ExprRef<BvSRemExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitBvURem(const ExprRef<BvURemExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitShl(const ExprRef<ShlExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitLShr(const ExprRef<LShrExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitAShr(const ExprRef<AShrExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitBvAnd(const ExprRef<BvAndExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitBvOr(const ExprRef<BvOrExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitBvXor(const ExprRef<BvXorExpr>& expr) {
    return EvalBinaryArithmetic(this, expr);
}

// Logic
//-----------------------------------------------------------------------------

ExprRef<LiteralExpr> ExprEvaluatorBase::visitNot(const ExprRef<NotExpr>& expr)
{
    auto boolLit = dyn_cast<BoolLiteralExpr>(visit(expr->getOperand()).get());
    return BoolLiteralExpr::Get(cast<BoolType>(expr->getType()), !boolLit->getValue());
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitAnd(const ExprRef<AndExpr>& expr)
{
    auto perform_and = [this](bool left, const ExprPtr& right) {
        bool rightVal = llvm::cast<BoolLiteralExpr>(visit(right))->getValue();
        return left && rightVal;
    };

    bool result = std::accumulate(expr->op_begin(), expr->op_end(), true, perform_and);

    return BoolLiteralExpr::Get(cast<BoolType>(expr->getType()), result);
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitOr(const ExprRef<OrExpr>& expr)
{
    auto perform_or = [this](bool left, const ExprPtr& right) {
        bool rightVal = llvm::cast<BoolLiteralExpr>(visit(right))->getValue();
        return left || rightVal;
    };

    bool result = std::accumulate(expr->op_begin(), expr->op_end(), false, perform_or);

    return BoolLiteralExpr::Get(cast<BoolType>(expr->getType()), result);
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitXor(const ExprRef<XorExpr>& expr) {
    auto left = cast<BoolLiteralExpr>(visit(expr->getLeft()))->getValue();
    auto right = cast<BoolLiteralExpr>(visit(expr->getRight()))->getValue();

    return BoolLiteralExpr::Get(cast<BoolType>(expr->getType()), left != right);
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitImply(const ExprRef<ImplyExpr>& expr) {
    auto left = cast<BoolLiteralExpr>(visit(expr->getLeft()))->getValue();
    auto right = cast<BoolLiteralExpr>(visit(expr->getRight()))->getValue();

    return BoolLiteralExpr::Get(cast<BoolType>(expr->getType()), !left || right);
}

template<Expr::ExprKind Kind>
static ExprRef<LiteralExpr> EvalBvCompare(
    ExprEvaluatorBase* visitor, 
    const ExprRef<CompareExpr<Kind>>& expr)
{
    static_assert(Expr::FirstCompare <= Kind && Kind <= Expr::LastCompare,
        "A compare expression must have a compare expression kind.");

    auto left = dyn_cast<BvLiteralExpr>(visitor->visit(expr->getLeft()).get())->getValue();
    auto right = dyn_cast<BvLiteralExpr>(visitor->visit(expr->getRight()).get())->getValue();

    BoolType& type = BoolType::Get(expr->getContext());

    switch (Kind) {
        case Expr::Eq: return BoolLiteralExpr::Get(type, left.eq(right));
        case Expr::NotEq: return BoolLiteralExpr::Get(type, left.ne(right));
        case Expr::SLt: return BoolLiteralExpr::Get(type, left.slt(right));
        case Expr::SLtEq: return BoolLiteralExpr::Get(type, left.sle(right));
        case Expr::SGt: return BoolLiteralExpr::Get(type, left.sgt(right));
        case Expr::SGtEq: return BoolLiteralExpr::Get(type, left.sge(right));
        case Expr::ULt: return BoolLiteralExpr::Get(type, left.ult(right));
        case Expr::ULtEq: return BoolLiteralExpr::Get(type, left.ule(right));
        case Expr::UGt: return BoolLiteralExpr::Get(type, left.ugt(right));
        case Expr::UGtEq: return BoolLiteralExpr::Get(type, left.uge(right));
    }

    llvm_unreachable("Unknown binary arithmetic expression kind.");
}

// Compare
ExprRef<LiteralExpr> ExprEvaluatorBase::visitEq(const ExprRef<EqExpr>& expr)
{
    auto left = visit(expr->getLeft());
    auto right = visit(expr->getRight());

    Type& opTy = left->getType();
    assert(left->getType() == right->getType());

    BoolType& boolTy = BoolType::Get(opTy.getContext());

    assert(!opTy.isFloatType() && "Float types must be compared using FEqExpr!");

    switch (opTy.getTypeID()) {
        case Type::BvTypeID:
            return EvalBvCompare(this, expr);
        case Type::BoolTypeID:
            return BoolLiteralExpr::Get(
                boolTy,
                cast<BoolLiteralExpr>(left)->getValue() == cast<BoolLiteralExpr>(right)->getValue()
            );
    }

    llvm_unreachable("Invalid operand type in an EqExpr");
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitNotEq(const ExprRef<NotEqExpr>& expr)
{
    return EvalBvCompare(this, expr);
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitSLt(const ExprRef<SLtExpr>& expr) {
    return EvalBvCompare(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitSLtEq(const ExprRef<SLtEqExpr>& expr) {
    return EvalBvCompare(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitSGt(const ExprRef<SGtExpr>& expr) {
    return EvalBvCompare(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitSGtEq(const ExprRef<SGtEqExpr>& expr) {
    return EvalBvCompare(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitULt(const ExprRef<ULtExpr>& expr) {
    return EvalBvCompare(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitULtEq(const ExprRef<ULtEqExpr>& expr) {
    return EvalBvCompare(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitUGt(const ExprRef<UGtExpr>& expr) {
    return EvalBvCompare(this, expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitUGtEq(const ExprRef<UGtEqExpr>& expr) {
    return EvalBvCompare(this, expr);
}

// Floating-point queries
ExprRef<LiteralExpr> ExprEvaluatorBase::visitFIsNan(const ExprRef<FIsNanExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitFIsInf(const ExprRef<FIsInfExpr>& expr) {
    return this->visitNonNullary(expr);
}

// Floating-point arithmetic
ExprRef<LiteralExpr> ExprEvaluatorBase::visitFAdd(const ExprRef<FAddExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitFSub(const ExprRef<FSubExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitFMul(const ExprRef<FMulExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitFDiv(const ExprRef<FDivExpr>& expr) {
    return this->visitNonNullary(expr);
}

// Floating-point compare
ExprRef<LiteralExpr> ExprEvaluatorBase::visitFEq(const ExprRef<FEqExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitFGt(const ExprRef<FGtExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitFGtEq(const ExprRef<FGtEqExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitFLt(const ExprRef<FLtExpr>& expr) {
    return this->visitNonNullary(expr);
}
ExprRef<LiteralExpr> ExprEvaluatorBase::visitFLtEq(const ExprRef<FLtEqExpr>& expr) {
    return this->visitNonNullary(expr);
}

// Ternary
ExprRef<LiteralExpr> ExprEvaluatorBase::visitSelect(const ExprRef<SelectExpr>& expr) {
    // TODO: Support Int and Float...
    auto cond = dyn_cast<BoolLiteralExpr>(visit(expr->getCondition()).get());
    auto then = dyn_cast<BvLiteralExpr>(visit(expr->getThen()).get());
    auto elze = dyn_cast<BvLiteralExpr>(visit(expr->getElse()).get());

    return BvLiteralExpr::Get(
        cast<BvType>(expr->getType()),
        cond->getValue() ? then->getValue() : elze->getValue()
    );
}

// Arrays
ExprRef<LiteralExpr> ExprEvaluatorBase::visitArrayRead(const ExprRef<ArrayReadExpr>& expr) {
    return this->visitNonNullary(expr);
}

ExprRef<LiteralExpr> ExprEvaluatorBase::visitArrayWrite(const ExprRef<ArrayWriteExpr>& expr) {
    return this->visitNonNullary(expr);
}