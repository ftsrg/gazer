#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/ADT/APInt.h>

using namespace gazer;

namespace
{

class DefaultExprBuilder : public ExprBuilder
{
public:
    DefaultExprBuilder(GazerContext& context)
        : ExprBuilder(context)
    {}

    ExprPtr Not(const ExprPtr& op) override {
        return NotExpr::Create(op);
    }
    ExprPtr ZExt(const ExprPtr& op, BvType& type) override {
        return ZExtExpr::Create(op, type);
    }
    ExprPtr SExt(const ExprPtr& op, BvType& type) override {
        return SExtExpr::Create(op, type);
    }
    ExprPtr Trunc(const ExprPtr& op, BvType& type) override {
        return ExtractExpr::Create(op, 0, type.getWidth());
    }
    ExprPtr Extract(const ExprPtr& op, unsigned offset, unsigned width) override {
        return ExtractExpr::Create(op, offset, width);
    }

    ExprPtr Add(const ExprPtr& left, const ExprPtr& right) override {
        return AddExpr::Create(left, right);
    }
    ExprPtr Sub(const ExprPtr& left, const ExprPtr& right) override {
        return SubExpr::Create(left, right);
    }
    ExprPtr Mul(const ExprPtr& left, const ExprPtr& right) override {
        return MulExpr::Create(left, right);
    }
    ExprPtr BvSDiv(const ExprPtr& left, const ExprPtr& right) override {
        return BvSDivExpr::Create(left, right);
    }
    ExprPtr BvUDiv(const ExprPtr& left, const ExprPtr& right) override {
        return BvUDivExpr::Create(left, right);
    }
    ExprPtr BvSRem(const ExprPtr& left, const ExprPtr& right) override {
        return BvSRemExpr::Create(left, right);
    }
    ExprPtr BvURem(const ExprPtr& left, const ExprPtr& right) override {
        return BvURemExpr::Create(left, right);
    }
    ExprPtr Shl(const ExprPtr& left, const ExprPtr& right) override {
        return ShlExpr::Create(left, right);
    }
    ExprPtr LShr(const ExprPtr& left, const ExprPtr& right) override {
        return LShrExpr::Create(left, right);
    }
    ExprPtr AShr(const ExprPtr& left, const ExprPtr& right) override {
        return AShrExpr::Create(left, right);
    }
    ExprPtr BvAnd(const ExprPtr& left, const ExprPtr& right) override {
        return BvAndExpr::Create(left, right);
    }
    ExprPtr BvOr(const ExprPtr& left, const ExprPtr& right) override {
        return BvOrExpr::Create(left, right);
    }
    ExprPtr BvXor(const ExprPtr& left, const ExprPtr& right) override {
        return BvXorExpr::Create(left, right);
    }

    ExprPtr And(const ExprVector& vector) override {
        return AndExpr::Create(vector);
    }
    ExprPtr Or(const ExprVector& vector) override {
        return OrExpr::Create(vector);
    }
    ExprPtr Xor(const ExprPtr& left, const ExprPtr& right) override {
        return XorExpr::Create(left, right);
    }
    ExprPtr Imply(const ExprPtr& left, const ExprPtr& right) override {
        return ImplyExpr::Create(left, right);
    }

    ExprPtr Eq(const ExprPtr& left, const ExprPtr& right) override {
        return EqExpr::Create(left, right);
    }
    
    //ExprPtr NotEq(const ExprPtr& left, const ExprPtr& right) override {
    //    return NotEqExpr::Create(left, right);
    //}

    ExprPtr BvSLt(const ExprPtr& left, const ExprPtr& right) override {
        return BvSLtExpr::Create(left, right);
    }
    ExprPtr BvSLtEq(const ExprPtr& left, const ExprPtr& right) override {
        return BvSLtEqExpr::Create(left, right);
    }
    ExprPtr BvSGt(const ExprPtr& left, const ExprPtr& right) override {
        return BvSGtExpr::Create(left, right);
    }
    ExprPtr BvSGtEq(const ExprPtr& left, const ExprPtr& right) override {
        return BvSGtEqExpr::Create(left, right);
    }

    ExprPtr BvULt(const ExprPtr& left, const ExprPtr& right) override {
        return BvULtExpr::Create(left, right);
    }
    ExprPtr BvULtEq(const ExprPtr& left, const ExprPtr& right) override {
        return BvULtEqExpr::Create(left, right);
    }
    ExprPtr BvUGt(const ExprPtr& left, const ExprPtr& right) override {
        return BvUGtExpr::Create(left, right);
    }
    ExprPtr BvUGtEq(const ExprPtr& left, const ExprPtr& right) override {
        return BvUGtEqExpr::Create(left, right);
    }

    ExprPtr Select(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze) override {
        return SelectExpr::Create(condition, then, elze);
    }

    ExprPtr FIsNan(const ExprPtr& op) override {
        return FIsNanExpr::Create(op);
    }
    ExprPtr FIsInf(const ExprPtr& op) override {
        return FIsInfExpr::Create(op);
    }
    
    ExprPtr FCast(const ExprPtr& op, FloatType& type, llvm::APFloat::roundingMode rm) override {
        return FCastExpr::Create(op, type, rm);
    }

    ExprPtr SignedToFp(const ExprPtr& op, FloatType& type, llvm::APFloat::roundingMode rm) override {
        return SignedToFpExpr::Create(op, type, rm);
    }

    ExprPtr UnsignedToFp(const ExprPtr& op, FloatType& type, llvm::APFloat::roundingMode rm) override {
        return UnsignedToFpExpr::Create(op, type, rm);
    }

    ExprPtr FpToSigned(const ExprPtr& op, BvType& type, llvm::APFloat::roundingMode rm) override {
        return FpToSignedExpr::Create(op, type, rm);
    }

    ExprPtr FpToUnsigned(const ExprPtr& op, BvType& type, llvm::APFloat::roundingMode rm) override {
        return FpToUnsignedExpr::Create(op, type, rm);
    }

    ExprPtr FAdd(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override {
        return FAddExpr::Create(left, right, rm);
    }
    ExprPtr FSub(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override {
        return FSubExpr::Create(left, right, rm);
    }
    ExprPtr FMul(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override {
        return FMulExpr::Create(left, right, rm);
    }
    ExprPtr FDiv(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override {
        return FDivExpr::Create(left, right, rm);
    }
    
    ExprPtr FEq(const ExprPtr& left, const ExprPtr& right) override {
        return FEqExpr::Create(left, right);
    }
    ExprPtr FGt(const ExprPtr& left, const ExprPtr& right) override {
        return FGtExpr::Create(left, right);
    }
    ExprPtr FGtEq(const ExprPtr& left, const ExprPtr& right) override {
        return FGtEqExpr::Create(left, right);
    }
    ExprPtr FLt(const ExprPtr& left, const ExprPtr& right) override {
        return FLtExpr::Create(left, right);
    }
    ExprPtr FLtEq(const ExprPtr& left, const ExprPtr& right) override {
        return FLtEqExpr::Create(left, right);
    }
};

} // end anonymous namespace

std::unique_ptr<ExprBuilder> gazer::CreateExprBuilder(GazerContext& context) {
    return std::unique_ptr<ExprBuilder>(new DefaultExprBuilder(context));
}
