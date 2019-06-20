#ifndef _GAZER_CORE_EXPR_EXPRBUILDER_H
#define _GAZER_CORE_EXPR_EXPRBUILDER_H

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
protected:
    ExprBuilder(GazerContext& context)
        : mContext(context)
    {}

public:
    GazerContext& getContext() const { return mContext; }

public:
    virtual ~ExprBuilder() {}

    //--- Literals ---//
    ExprRef<BvLiteralExpr> BvLit(uint64_t value, unsigned bits) {
        return BvLiteralExpr::Get(BvType::Get(mContext, bits), llvm::APInt(bits, value));
    }
    ExprRef<BvLiteralExpr> BvLit(llvm::APInt value) {
        return BvLiteralExpr::Get(BvType::Get(mContext, value.getBitWidth()), value);
    }

    ExprRef<IntLiteralExpr> IntLit(int64_t value) {
        return IntLiteralExpr::Get(IntType::Get(mContext), value);
    }

    ExprRef<BoolLiteralExpr> BoolLit(bool value) { return value ? True() : False(); }
    ExprRef<BoolLiteralExpr> True()  { return BoolLiteralExpr::True(BoolType::Get(mContext)); }
    ExprRef<BoolLiteralExpr> False() { return BoolLiteralExpr::False(BoolType::Get(mContext)); }
    ExprRef<UndefExpr> Undef(Type& type) { return UndefExpr::Get(type); }

    ExprRef<FloatLiteralExpr> FloatLit(llvm::APFloat value) {
        auto numbits = llvm::APFloat::getSizeInBits(value.getSemantics());
        auto& type = FloatType::Get(mContext, static_cast<FloatType::FloatPrecision>(numbits));

        return FloatLiteralExpr::Get(type, value);
    }

    //--- Unary ---//
    virtual ExprPtr Not(const ExprPtr& op) = 0;
    virtual ExprPtr ZExt(const ExprPtr& op, BvType& type) = 0;
    virtual ExprPtr SExt(const ExprPtr& op, BvType& type) = 0;
    virtual ExprPtr Trunc(const ExprPtr& op, BvType& type) = 0;
    virtual ExprPtr Extract(const ExprPtr& op, unsigned offset, unsigned width) = 0;

    //--- Binary ---//
    virtual ExprPtr Add(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Sub(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Mul(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr SDiv(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr UDiv(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr SRem(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr URem(const ExprPtr& left, const ExprPtr& right) = 0;

    virtual ExprPtr Shl(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr LShr(const ExprPtr& left, const ExprPtr& right) = 0;    
    virtual ExprPtr AShr(const ExprPtr& left, const ExprPtr& right) = 0;    
    virtual ExprPtr BAnd(const ExprPtr& left, const ExprPtr& right) = 0;    
    virtual ExprPtr BOr(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr BXor(const ExprPtr& left, const ExprPtr& right) = 0;    

    //--- Logic ---//
    virtual ExprPtr And(const ExprVector& vector) = 0;
    virtual ExprPtr Or(const ExprVector& vector) = 0;

    ExprPtr And(const ExprPtr& left, const ExprPtr& right) {
        return this->And({left, right});
    }
    ExprPtr Or(const ExprPtr& left, const ExprPtr& right) {
        return this->Or({left, right});
    }

    template<class InputIterator>
    ExprPtr And(InputIterator begin, InputIterator end) {
        return this->And(ExprVector(begin, end));
    }
    template<class InputIterator>
    ExprPtr Or(InputIterator begin, InputIterator end) {
        return this->Or(ExprVector(begin, end));
    }

    virtual ExprPtr Xor(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr Imply(const ExprPtr& left, const ExprPtr& right) = 0;

    //--- Compare ---//
    virtual ExprPtr Eq(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr NotEq(const ExprPtr& left, const ExprPtr& right) = 0;

    virtual ExprPtr SLt(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr SLtEq(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr SGt(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr SGtEq(const ExprPtr& left, const ExprPtr& right) = 0;

    virtual ExprPtr ULt(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr ULtEq(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr UGt(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr UGtEq(const ExprPtr& left, const ExprPtr& right) = 0;

    //--- Floating point ---//
    virtual ExprPtr FIsNan(const ExprPtr& op) = 0;
    virtual ExprPtr FIsInf(const ExprPtr& op) = 0;
    
    virtual ExprPtr FAdd(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) = 0;
    virtual ExprPtr FSub(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) = 0;
    virtual ExprPtr FMul(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) = 0;
    virtual ExprPtr FDiv(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) = 0;
    
    virtual ExprPtr FEq(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr FGt(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr FGtEq(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr FLt(const ExprPtr& left, const ExprPtr& right) = 0;
    virtual ExprPtr FLtEq(const ExprPtr& left, const ExprPtr& right) = 0;

    //--- Ternary ---//
    virtual ExprPtr Select(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze) = 0;
private:
    GazerContext& mContext;
};

std::unique_ptr<ExprBuilder> CreateExprBuilder(GazerContext& context);
std::unique_ptr<ExprBuilder> CreateFoldingExprBuilder(GazerContext& context);

}

#endif
