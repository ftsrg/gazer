#include "gazer/Core/Expr/ConstantFolder.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"
//#include "gazer/Core/Expr/Matcher.h"

using namespace gazer;
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

}

ExprPtr ConstantFolder::Sub(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::Mul(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::SDiv(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::UDiv(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::SRem(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::URem(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::Shl(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::LShr(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::AShr(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::BAnd(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::BOr(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::BXor(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::And(const ExprVector& vector)
{

}

ExprPtr ConstantFolder::Or(const ExprVector& vector)
{

}

ExprPtr ConstantFolder::Xor(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::Eq(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::NotEq(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::SLt(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::SLtEq(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::SGt(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::SGtEq(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::ULt(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::ULtEq(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::UGt(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::UGtEq(const ExprPtr& left, const ExprPtr& right)
{

}


//--- Floating point ---//
ExprPtr ConstantFolder::FIsNan(const ExprPtr& op)
{

}

ExprPtr ConstantFolder::FIsInf(const ExprPtr& op)
{

}

ExprPtr ConstantFolder::FAdd(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm)
{

}

ExprPtr ConstantFolder::FSub(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm)
{

}

ExprPtr ConstantFolder::FMul(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm)
{

}

ExprPtr ConstantFolder::FDiv(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm)
{

}


ExprPtr ConstantFolder::FEq(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::FGt(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::FGtEq(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::FLt(const ExprPtr& left, const ExprPtr& right)
{

}

ExprPtr ConstantFolder::FLtEq(const ExprPtr& left, const ExprPtr& right)
{

}


//--- Ternary ---//
ExprPtr ConstantFolder::Select(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze)
{

}

