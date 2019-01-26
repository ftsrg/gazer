#include "gazer/Core/Expr/ConstantFolder.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Expr/Matcher.h"

using namespace gazer;
using namespace gazer::PatternMatch;
using llvm::dyn_cast;
using llvm::cast;

ExprPtr ConstantFolder::Not(const ExprPtr& op)
{
    if (auto boolLit = dyn_cast<BoolLiteralExpr>(op.get())) {
        return BoolLiteralExpr::Get(!boolLit->getValue());
    }

    return NotExpr::Create(op);
}

ExprPtr ConstantFolder::ZExt(const ExprPtr& op, const BvType& type)
{
    return ZExtExpr::Create(op, type);
}

ExprPtr ConstantFolder::SExt(const ExprPtr& op, const BvType& type)
{
    return SExtExpr::Create(op, type);
}

ExprPtr ConstantFolder::Trunc(const ExprPtr& op, const BvType& type)
{
    return ExtractExpr::Create(op, 0, type.getWidth());
}

ExprPtr ConstantFolder::Extract(const ExprPtr& op, unsigned offset, unsigned width)
{
    return ExtractExpr::Create(op, offset, width);
}

ExprPtr ConstantFolder::Add(const ExprPtr& left, const ExprPtr& right)
{
    return AddExpr::Create(left, right);
}

ExprPtr ConstantFolder::Sub(const ExprPtr& left, const ExprPtr& right)
{
    return SubExpr::Create(left, right);
}

ExprPtr ConstantFolder::Mul(const ExprPtr& left, const ExprPtr& right)
{
    return SubExpr::Create(left, right);
}

ExprPtr ConstantFolder::SDiv(const ExprPtr& left, const ExprPtr& right)
{
    return SDivExpr::Create(left, right);
}

ExprPtr ConstantFolder::UDiv(const ExprPtr& left, const ExprPtr& right)
{
    return UDivExpr::Create(left, right);
}

ExprPtr ConstantFolder::SRem(const ExprPtr& left, const ExprPtr& right)
{
    return SRemExpr::Create(left, right);
}

ExprPtr ConstantFolder::URem(const ExprPtr& left, const ExprPtr& right)
{
    return URemExpr::Create(left, right);
}

ExprPtr ConstantFolder::Shl(const ExprPtr& left, const ExprPtr& right)
{
    return ShlExpr::Create(left, right);
}

ExprPtr ConstantFolder::LShr(const ExprPtr& left, const ExprPtr& right)
{
    return LShrExpr::Create(left, right);
}

ExprPtr ConstantFolder::AShr(const ExprPtr& left, const ExprPtr& right)
{
    return AShrExpr::Create(left, right);
}

ExprPtr ConstantFolder::BAnd(const ExprPtr& left, const ExprPtr& right)
{
    return BAndExpr::Create(left, right);
}

ExprPtr ConstantFolder::BOr(const ExprPtr& left, const ExprPtr& right)
{
    return BOrExpr::Create(left, right);
}

ExprPtr ConstantFolder::BXor(const ExprPtr& left, const ExprPtr& right)
{
    return BXorExpr::Create(left, right);
}

ExprPtr ConstantFolder::And(const ExprVector& vector)
{
    return AndExpr::Create(vector);
}

ExprPtr ConstantFolder::Or(const ExprVector& vector)
{
    return OrExpr::Create(vector);
}

ExprPtr ConstantFolder::Xor(const ExprPtr& left, const ExprPtr& right)
{
    return XorExpr::Create(left, right);
}

ExprPtr ConstantFolder::Eq(const ExprPtr& left, const ExprPtr& right)
{
    if (auto c1 = dyn_cast<LiteralExpr>(left.get())) {
        if (auto c2 = dyn_cast<LiteralExpr>(right.get())) {
            assert(c1->getType() == c2->getType() && "Equals expression operand types must match!");
            return BoolLiteralExpr::Get(c1->equals(*c2));
        }
    }

    if (auto v1 = dyn_cast<VarRefExpr>(left.get())) {
        if (auto v2 = dyn_cast<VarRefExpr>(right.get())) {
            if (v1->getVariable() == v2->getVariable()) {
                return BoolLiteralExpr::getTrue();
            }
        }
    }

    return EqExpr::Create(left, right);
}

ExprPtr ConstantFolder::NotEq(const ExprPtr& left, const ExprPtr& right)
{
    return NotEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::SLt(const ExprPtr& left, const ExprPtr& right)
{
    return SLtExpr::Create(left, right);
}

ExprPtr ConstantFolder::SLtEq(const ExprPtr& left, const ExprPtr& right)
{
    return SLtEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::SGt(const ExprPtr& left, const ExprPtr& right)
{
    return SGtExpr::Create(left, right);
}

ExprPtr ConstantFolder::SGtEq(const ExprPtr& left, const ExprPtr& right)
{
    return SGtEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::ULt(const ExprPtr& left, const ExprPtr& right)
{
    return ULtExpr::Create(left, right);
}

ExprPtr ConstantFolder::ULtEq(const ExprPtr& left, const ExprPtr& right)
{
    return ULtEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::UGt(const ExprPtr& left, const ExprPtr& right)
{
    return UGtExpr::Create(left, right);
}

ExprPtr ConstantFolder::UGtEq(const ExprPtr& left, const ExprPtr& right)
{
    return UGtEqExpr::Create(left, right);
}

//--- Floating point ---//
ExprPtr ConstantFolder::FIsNan(const ExprPtr& op)
{
    return FIsNanExpr::Create(op);
}

ExprPtr ConstantFolder::FIsInf(const ExprPtr& op)
{
    return FIsInfExpr::Create(op);
}

ExprPtr ConstantFolder::FAdd(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm)
{
    return FAddExpr::Create(left, right, rm);
}

ExprPtr ConstantFolder::FSub(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm)
{
    return FSubExpr::Create(left, right, rm);
}

ExprPtr ConstantFolder::FMul(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm)
{
    return FMulExpr::Create(left, right, rm);
}

ExprPtr ConstantFolder::FDiv(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm)
{
    return FDivExpr::Create(left, right, rm);
}

ExprPtr ConstantFolder::FEq(const ExprPtr& left, const ExprPtr& right)
{
    return FEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::FGt(const ExprPtr& left, const ExprPtr& right)
{
    return FGtExpr::Create(left, right);
}

ExprPtr ConstantFolder::FGtEq(const ExprPtr& left, const ExprPtr& right)
{
    return FGtEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::FLt(const ExprPtr& left, const ExprPtr& right)
{
    return FLtExpr::Create(left, right);
}

ExprPtr ConstantFolder::FLtEq(const ExprPtr& left, const ExprPtr& right)
{
    return FLtEqExpr::Create(left, right);
}

//--- Ternary ---//
ExprPtr ConstantFolder::Select(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze)
{
    return SelectExpr::Create(condition, then, elze);
}
