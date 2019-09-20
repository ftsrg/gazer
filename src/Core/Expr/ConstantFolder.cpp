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
        return BoolLiteralExpr::Get(op->getContext(), !boolLit->getValue());
    }

    return NotExpr::Create(op);
}

ExprPtr ConstantFolder::ZExt(const ExprPtr& op, BvType& type)
{
    if (auto bvLit = dyn_cast<BvLiteralExpr>(op.get())) {
        return BvLiteralExpr::Get(type, bvLit->getValue().zext(type.getWidth()));
    }

    return ZExtExpr::Create(op, type);
}

ExprPtr ConstantFolder::SExt(const ExprPtr& op, BvType& type)
{
    if (auto bvLit = dyn_cast<BvLiteralExpr>(op.get())) {
        return BvLiteralExpr::Get(type, bvLit->getValue().sext(type.getWidth()));
    }

    return SExtExpr::Create(op, type);
}

ExprPtr ConstantFolder::Trunc(const ExprPtr& op, BvType& type)
{
    return ExtractExpr::Create(op, 0, type.getWidth());
}

ExprPtr ConstantFolder::Extract(const ExprPtr& op, unsigned offset, unsigned width)
{
    if (auto bvLit = dyn_cast<BvLiteralExpr>(op)) {
        return BvLiteralExpr::Get(
            BvType::Get(op->getContext(), width),
            bvLit->getValue().extractBits(width, offset)
        );
    }

    return ExtractExpr::Create(op, offset, width);
}

ExprPtr ConstantFolder::Add(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (lhsLit->getValue() == llvm::APInt(lhsLit->getType().getWidth(), 0)) {
            return right;
        }

        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue() + rhsLit->getValue()
            );
        }
    } else if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
        if (rhsLit->getValue() == llvm::APInt(rhsLit->getType().getWidth(), 0)) {
            return left;
        }
    } else if (auto lhsLit = dyn_cast<IntLiteralExpr>(left)) {
        if (lhsLit->getValue() == 0) {
            return right;
        }

        if (auto rhsLit = dyn_cast<IntLiteralExpr>(right)) {
            return IntLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue() + rhsLit->getValue()
            );
        }
    } else if (auto rhsLit = dyn_cast<IntLiteralExpr>(right)) {
        if (rhsLit->getValue() == 0) {
            return left;
        }
    } 

    return AddExpr::Create(left, right);
}

ExprPtr ConstantFolder::Sub(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue() - rhsLit->getValue()
            );
        }
    }

    return SubExpr::Create(left, right);
}

ExprPtr ConstantFolder::Mul(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue() * rhsLit->getValue()
            );
        }
    }

    return SubExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvSDiv(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue().sdiv(rhsLit->getValue())
            );
        }
    }

    return BvSDivExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvUDiv(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue().udiv(rhsLit->getValue())
            );
        }
    }

    return BvUDivExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvSRem(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue().srem(rhsLit->getValue())
            );
        }
    }

    return BvSRemExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvURem(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue().urem(rhsLit->getValue())
            );
        }
    }

    return BvURemExpr::Create(left, right);
}

ExprPtr ConstantFolder::Shl(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue().shl(rhsLit->getValue())
            );
        }
    }

    return ShlExpr::Create(left, right);
}

ExprPtr ConstantFolder::LShr(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue().lshr(rhsLit->getValue())
            );
        }
    }

    return LShrExpr::Create(left, right);
}

ExprPtr ConstantFolder::AShr(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue().ashr(rhsLit->getValue())
            );
        }
    }

    return AShrExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvAnd(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue() & rhsLit->getValue()
            );
        }
    }

    return BvAndExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvOr(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue() | rhsLit->getValue()
            );
        }
    }

    return BvOrExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvXor(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BvLiteralExpr::Get(
                lhsLit->getType(),
                lhsLit->getValue() ^ rhsLit->getValue()
            );
        }
    }

    return BvXorExpr::Create(left, right);
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

ExprPtr ConstantFolder::Imply(const ExprPtr& left, const ExprPtr& right)
{
    return ImplyExpr::Create(left, right);
}

ExprPtr ConstantFolder::Eq(const ExprPtr& left, const ExprPtr& right)
{
    if (left == right) {
        return BoolLiteralExpr::True(left->getContext());
    }

    if (auto c1 = dyn_cast<LiteralExpr>(left.get())) {
        if (auto c2 = dyn_cast<LiteralExpr>(right.get())) {
            assert(c1->getType() == c2->getType() && "Equals expression operand types must match!");
            return BoolLiteralExpr::Get(left->getContext(), c1 == c2);
        }
    }

    if (auto v1 = dyn_cast<VarRefExpr>(left.get())) {
        if (auto v2 = dyn_cast<VarRefExpr>(right.get())) {
            if (v1->getVariable() == v2->getVariable()) {
                return BoolLiteralExpr::True(left->getContext());
            }
        }
    }

    return EqExpr::Create(left, right);
}

ExprPtr ConstantFolder::NotEq(const ExprPtr& left, const ExprPtr& right)
{
    if (left == right) {
        return BoolLiteralExpr::False(left->getContext());
    }

    return NotEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::Lt(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = dyn_cast<IntLiteralExpr>(left)) {
        if (auto rhsLit = dyn_cast<IntLiteralExpr>(right)) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue() < rhsLit->getValue());
        }
    }

    return LtExpr::Create(left, right);
}

ExprPtr ConstantFolder::LtEq(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = dyn_cast<IntLiteralExpr>(left)) {
        if (auto rhsLit = dyn_cast<IntLiteralExpr>(right)) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue() <= rhsLit->getValue());
        }
    }

    return LtEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::Gt(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = dyn_cast<IntLiteralExpr>(left)) {
        if (auto rhsLit = dyn_cast<IntLiteralExpr>(right)) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue() > rhsLit->getValue());
        }
    }

    return GtExpr::Create(left, right);
}

ExprPtr ConstantFolder::GtEq(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = dyn_cast<IntLiteralExpr>(left)) {
        if (auto rhsLit = dyn_cast<IntLiteralExpr>(right)) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue() >= rhsLit->getValue());
        }
    }

    return GtEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvSLt(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue().slt(rhsLit->getValue()));
        }
    }

    return BvSLtExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvSLtEq(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue().sle(rhsLit->getValue()));
        }
    }

    return BvSLtEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvSGt(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue().sgt(rhsLit->getValue()));
        }
    }

    return BvSGtExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvSGtEq(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue().sge(rhsLit->getValue()));
        }
    }

    return BvSGtEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvULt(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue().ult(rhsLit->getValue()));
        }
    }

    return BvULtExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvULtEq(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue().ule(rhsLit->getValue()));
        }
    }

    return BvULtEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvUGt(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue().ugt(rhsLit->getValue()));
        }
    }

    return BvUGtExpr::Create(left, right);
}

ExprPtr ConstantFolder::BvUGtEq(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
        if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue().uge(rhsLit->getValue()));
        }
    }

    return BvUGtEqExpr::Create(left, right);
}

//--- Floating point ---//
ExprPtr ConstantFolder::FIsNan(const ExprPtr& op)
{
    if (op->getKind() == Expr::Literal) {
        auto fltLit = llvm::dyn_cast<FloatLiteralExpr>(op.get());
        return BoolLiteralExpr::Get(BoolType::Get(op->getContext()), fltLit->getValue().isNaN());
    }

    return FIsNanExpr::Create(op);
}

ExprPtr ConstantFolder::FIsInf(const ExprPtr& op)
{
    if (op->getKind() == Expr::Literal) {
        auto fltLit = llvm::dyn_cast<FloatLiteralExpr>(op.get());
        return BoolLiteralExpr::Get(BoolType::Get(op->getContext()), fltLit->getValue().isInfinity());
    }

    return FIsInfExpr::Create(op);
}

ExprPtr ConstantFolder::FAdd(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm)
{
    if (left->getKind() == Expr::Literal && right->getKind() == Expr::Literal) {
        auto fltLeft  = llvm::cast<FloatLiteralExpr>(left.get());
        auto fltRight = llvm::cast<FloatLiteralExpr>(right.get());

        llvm::APFloat result(fltLeft->getValue());
        result.add(fltRight->getValue(), rm);

        return FloatLiteralExpr::Get(
            *llvm::cast<FloatType>(&left->getType()), result
        );
    }

    return FAddExpr::Create(left, right, rm);
}

ExprPtr ConstantFolder::FSub(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm)
{
    if (left->getKind() == Expr::Literal && right->getKind() == Expr::Literal) {
        auto fltLeft  = llvm::cast<FloatLiteralExpr>(left.get());
        auto fltRight = llvm::cast<FloatLiteralExpr>(right.get());

        llvm::APFloat result(fltLeft->getValue());
        result.subtract(fltRight->getValue(), rm);

        return FloatLiteralExpr::Get(
            *llvm::cast<FloatType>(&left->getType()), result
        );
    }
    return FSubExpr::Create(left, right, rm);
}

ExprPtr ConstantFolder::FMul(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm)
{
    if (left->getKind() == Expr::Literal && right->getKind() == Expr::Literal) {
        auto fltLeft  = llvm::cast<FloatLiteralExpr>(left.get());
        auto fltRight = llvm::cast<FloatLiteralExpr>(right.get());

        llvm::APFloat result(fltLeft->getValue());
        result.multiply(fltRight->getValue(), rm);

        return FloatLiteralExpr::Get(
            *llvm::cast<FloatType>(&left->getType()), result
        );
    }

    return FMulExpr::Create(left, right, rm);
}

ExprPtr ConstantFolder::FDiv(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm)
{
    if (left->getKind() == Expr::Literal && right->getKind() == Expr::Literal) {
        auto fltLeft  = llvm::cast<FloatLiteralExpr>(left.get());
        auto fltRight = llvm::cast<FloatLiteralExpr>(right.get());

        llvm::APFloat result(fltLeft->getValue());
        result.divide(fltRight->getValue(), rm);

        return FloatLiteralExpr::Get(
            *llvm::cast<FloatType>(&left->getType()), result
        );
    }

    return FDivExpr::Create(left, right, rm);
}

ExprPtr ConstantFolder::FEq(const ExprPtr& left, const ExprPtr& right)
{
    if (left->getKind() == Expr::Literal && right->getKind() == Expr::Literal) {
        auto fltLeft  = llvm::cast<FloatLiteralExpr>(left.get());
        auto fltRight = llvm::cast<FloatLiteralExpr>(right.get());

        return BoolLiteralExpr::Get(left->getContext(), fltLeft->getValue().compare(fltRight->getValue()) == llvm::APFloat::cmpEqual);
    }

    return FEqExpr::Create(left, right);
}

ExprPtr ConstantFolder::FGt(const ExprPtr& left, const ExprPtr& right)
{
    if (auto lhsLit = llvm::dyn_cast<FloatLiteralExpr>(left)) {
        if (auto rhsLit = dyn_cast<FloatLiteralExpr>(right)) {
            return BoolLiteralExpr::Get(left->getContext(), lhsLit->getValue().compare(rhsLit->getValue()) == llvm::APFloat::cmpGreaterThan );
        }
    }

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
