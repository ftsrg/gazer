#ifndef GAZER_CORE_EXPR_EXPRBUILDER_H
#define GAZER_CORE_EXPR_EXPRBUILDER_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

namespace llvm {
    class APInt;
}

namespace gazer
{

class ExprBuilder
{
public:
    explicit ExprBuilder(GazerContext& context)
        : mContext(context)
    {}

    [[nodiscard]] GazerContext& getContext() const { return mContext; }

public:
    virtual ~ExprBuilder() = default;

    //--- Literals ---//
    ExprRef<BvLiteralExpr> BvLit(uint64_t value, unsigned bits) {
        return BvLiteralExpr::Get(BvType::Get(mContext, bits), llvm::APInt(bits, value));
    }

    ExprRef<BvLiteralExpr> BvLit32(uint64_t value) { return BvLit(value, 32); }
    ExprRef<BvLiteralExpr> BvLit64(uint64_t value) { return BvLit(value, 64); }

    ExprRef<BvLiteralExpr> BvLit(const llvm::APInt& value) {
        return BvLiteralExpr::Get(BvType::Get(mContext, value.getBitWidth()), value);
    }

    ExprRef<IntLiteralExpr> IntLit(int64_t value) {
        return IntLiteralExpr::Get(IntType::Get(mContext), value);
    }

    ExprRef<BoolLiteralExpr> BoolLit(bool value) { return value ? True() : False(); }
    ExprRef<BoolLiteralExpr> True()  { return BoolLiteralExpr::True(BoolType::Get(mContext)); }
    ExprRef<BoolLiteralExpr> False() { return BoolLiteralExpr::False(BoolType::Get(mContext)); }
    ExprRef<UndefExpr> Undef(Type& type) { return UndefExpr::Get(type); }

    ExprRef<FloatLiteralExpr> FloatLit(const llvm::APFloat& value) {
        auto numbits = llvm::APFloat::getSizeInBits(value.getSemantics());
        auto& type = FloatType::Get(mContext, static_cast<FloatType::FloatPrecision>(numbits));

        return FloatLiteralExpr::Get(type, value);
    }

    virtual ExprPtr Not(const ExprPtr& op) {
        return NotExpr::Create(op);
    }
    
    virtual ExprPtr ZExt(const ExprPtr& op, BvType& type) {
        return ZExtExpr::Create(op, type);
    }
    
    virtual ExprPtr SExt(const ExprPtr& op, BvType& type) {
        return SExtExpr::Create(op, type);
    }
    
    virtual ExprPtr Trunc(const ExprPtr& op, BvType& type) {
        return ExtractExpr::Create(op, 0, type.getWidth());
    }

    virtual ExprPtr Extract(const ExprPtr& op, unsigned offset, unsigned width) {
        return ExtractExpr::Create(op, offset, width);
    }

    //--- Binary ---//
    virtual ExprPtr Add(const ExprPtr& left, const ExprPtr& right) {
        return AddExpr::Create(left, right);
    }
    virtual ExprPtr Sub(const ExprPtr& left, const ExprPtr& right) {
        return SubExpr::Create(left, right);
    }
    virtual ExprPtr Mul(const ExprPtr& left, const ExprPtr& right) {
        return MulExpr::Create(left, right);
    }
    virtual ExprPtr Div(const ExprPtr& left, const ExprPtr& right) {
        return DivExpr::Create(left, right);
    }
    virtual ExprPtr Mod(const ExprPtr& left, const ExprPtr& right) {
        return ModExpr::Create(left, right);
    }
    virtual ExprPtr Rem(const ExprPtr& left, const ExprPtr& right) {
        return RemExpr::Create(left, right);
    }

    virtual ExprPtr BvSDiv(const ExprPtr& left, const ExprPtr& right) {
        return BvSDivExpr::Create(left, right);
    }
    virtual ExprPtr BvUDiv(const ExprPtr& left, const ExprPtr& right) {
        return BvUDivExpr::Create(left, right);
    }
    virtual ExprPtr BvSRem(const ExprPtr& left, const ExprPtr& right) {
        return BvSRemExpr::Create(left, right);
    }
    virtual ExprPtr BvURem(const ExprPtr& left, const ExprPtr& right) {
        return BvURemExpr::Create(left, right);
    }

    virtual ExprPtr Shl(const ExprPtr& left, const ExprPtr& right) {
        return ShlExpr::Create(left, right);
    }
    virtual ExprPtr LShr(const ExprPtr& left, const ExprPtr& right) {
        return LShrExpr::Create(left, right);
    }
    virtual ExprPtr AShr(const ExprPtr& left, const ExprPtr& right) {
        return AShrExpr::Create(left, right);
    }
    virtual ExprPtr BvAnd(const ExprPtr& left, const ExprPtr& right) {
        return BvAndExpr::Create(left, right);
    }
    virtual ExprPtr BvOr(const ExprPtr& left, const ExprPtr& right) {
        return BvOrExpr::Create(left, right);
    }
    virtual ExprPtr BvXor(const ExprPtr& left, const ExprPtr& right) {
        return BvXorExpr::Create(left, right);
    }

    //--- Logic ---//
    virtual ExprPtr And(const ExprVector& vector) { return AndExpr::Create(vector); }
    virtual ExprPtr Or(const ExprVector& vector) { return OrExpr::Create(vector); }

    template<class Left, class Right>
    ExprPtr And(const ExprRef<Left>& left, const ExprRef<Right>& right) {
        return this->And({left, right});
    }

    template<class Left, class Right>
    ExprPtr Or(const ExprRef<Left>& left, const ExprRef<Right>& right) {
        return this->Or({left, right});
    }

    // The declarations above and below may clash in certain cases,
    // we use SFINAE to enable the iterator-based overloads only if
    // the type has iterator_traits.

    template<class InputIterator, class = typename std::iterator_traits<InputIterator>::value_type>
    ExprPtr And(InputIterator begin, InputIterator end) {
        return this->And(ExprVector(begin, end));
    }
    template<class InputIterator, class = typename std::iterator_traits<InputIterator>::value_type>
    ExprPtr Or(InputIterator begin, InputIterator end) {
        return this->Or(ExprVector(begin, end));
    }

    virtual ExprPtr Xor(const ExprPtr& left, const ExprPtr& right) {
        return XorExpr::Create(left, right);
    }
    virtual ExprPtr Imply(const ExprPtr& left, const ExprPtr& right) {
        return ImplyExpr::Create(left, right);
    }

    //--- Compare ---//
    virtual ExprPtr NotEq(const ExprPtr& left, const ExprPtr& right) {
        return this->Not(this->Eq(left, right));
    }

    virtual ExprPtr Eq(const ExprPtr& left, const ExprPtr& right) {
        return EqExpr::Create(left, right);
    }

    virtual ExprPtr Lt(const ExprPtr& left, const ExprPtr& right) {
        return LtExpr::Create(left, right);
    }
    virtual ExprPtr LtEq(const ExprPtr& left, const ExprPtr& right) {
        return LtEqExpr::Create(left, right);
    }
    virtual ExprPtr Gt(const ExprPtr& left, const ExprPtr& right) {
        return GtExpr::Create(left, right);
    }
    virtual ExprPtr GtEq(const ExprPtr& left, const ExprPtr& right) {
        return GtEqExpr::Create(left, right);
    }

    virtual ExprPtr BvSLt(const ExprPtr& left, const ExprPtr& right) {
        return BvSLtExpr::Create(left, right);
    }
    virtual ExprPtr BvSLtEq(const ExprPtr& left, const ExprPtr& right) {
        return BvSLtEqExpr::Create(left, right);
    }
    virtual ExprPtr BvSGt(const ExprPtr& left, const ExprPtr& right) {
        return BvSGtExpr::Create(left, right);
    }
    virtual ExprPtr BvSGtEq(const ExprPtr& left, const ExprPtr& right) {
        return BvSGtEqExpr::Create(left, right);
    }

    virtual ExprPtr BvULt(const ExprPtr& left, const ExprPtr& right) {
        return BvULtExpr::Create(left, right);
    }
    virtual ExprPtr BvULtEq(const ExprPtr& left, const ExprPtr& right) {
        return BvULtEqExpr::Create(left, right);
    }
    virtual ExprPtr BvUGt(const ExprPtr& left, const ExprPtr& right) {
        return BvUGtExpr::Create(left, right);
    }
    virtual ExprPtr BvUGtEq(const ExprPtr& left, const ExprPtr& right) {
        return BvUGtEqExpr::Create(left, right);
    }

    //--- Floating point ---//
    virtual ExprPtr FCast(const ExprPtr& op, FloatType& type, llvm::APFloat::roundingMode rm) {
        return FCastExpr::Create(op, type, rm);
    }

    virtual ExprPtr FIsNan(const ExprPtr& op) { return FIsNanExpr::Create(op); }
    virtual ExprPtr FIsInf(const ExprPtr& op) { return FIsInfExpr::Create(op); }
    
    virtual ExprPtr SignedToFp(const ExprPtr& op, FloatType& type, llvm::APFloat::roundingMode rm) {
        return SignedToFpExpr::Create(op, type, rm);
    }
    virtual ExprPtr UnsignedToFp(const ExprPtr& op, FloatType& type, llvm::APFloat::roundingMode rm) {
        return UnsignedToFpExpr::Create(op, type, rm);
    }
    virtual ExprPtr FpToSigned(const ExprPtr& op, BvType& type, llvm::APFloat::roundingMode rm) {
        return FpToSignedExpr::Create(op, type, rm);
    }
    virtual ExprPtr FpToUnsigned(const ExprPtr& op, BvType& type, llvm::APFloat::roundingMode rm) {
        return FpToUnsignedExpr::Create(op, type, rm);
    }

    virtual ExprPtr FAdd(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) {
        return FAddExpr::Create(left, right, rm);
    }
    virtual ExprPtr FSub(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) {
        return FSubExpr::Create(left, right, rm);
    }
    virtual ExprPtr FMul(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) {
        return FMulExpr::Create(left, right, rm);
    }
    virtual ExprPtr FDiv(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) {
        return FDivExpr::Create(left, right, rm);
    }
    
    virtual ExprPtr FEq(const ExprPtr& left, const ExprPtr& right)      { return FEqExpr::Create(left, right);      }
    virtual ExprPtr FGt(const ExprPtr& left, const ExprPtr& right)      { return FGtExpr::Create(left, right);      }
    virtual ExprPtr FGtEq(const ExprPtr& left, const ExprPtr& right)    { return FGtEqExpr::Create(left, right);    }
    virtual ExprPtr FLt(const ExprPtr& left, const ExprPtr& right)      { return FLtExpr::Create(left, right);      }
    virtual ExprPtr FLtEq(const ExprPtr& left, const ExprPtr& right)    { return FLtEqExpr::Create(left, right);    }

    //--- Ternary ---//
    virtual ExprPtr Select(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze) {
        return SelectExpr::Create(condition, then, elze);
    }
private:
    GazerContext& mContext;
};

std::unique_ptr<ExprBuilder> CreateExprBuilder(GazerContext& context);
std::unique_ptr<ExprBuilder> CreateFoldingExprBuilder(GazerContext& context);

}

#endif
