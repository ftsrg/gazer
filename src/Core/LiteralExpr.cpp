#include "gazer/Core/LiteralExpr.h"
#include "gazer/Support/DenseMapKeyInfo.h"

#include "GazerContextImpl.h"

#include <llvm/IR/Constants.h>
#include <llvm/ADT/DenseMap.h>

#include <llvm/Support/raw_ostream.h>

#include <map>

using namespace gazer;

ExprRef<UndefExpr> UndefExpr::Get(Type& type)
{
    return type.getContext().pImpl->Exprs.create<UndefExpr>(type);
}

ExprRef<BoolLiteralExpr> BoolLiteralExpr::True(BoolType& type) {
    return type.getContext().pImpl->TrueLit;
}

ExprRef<BoolLiteralExpr> BoolLiteralExpr::False(BoolType& type) {
    return type.getContext().pImpl->FalseLit;
}

ExprRef<IntLiteralExpr> IntLiteralExpr::Get(IntType& type, int64_t value)
{
    return type.getContext().pImpl->Exprs.create<IntLiteralExpr>(type, value);
}

ExprRef<RealLiteralExpr> RealLiteralExpr::Get(RealType& type, boost::rational<int64_t> value)
{
    return type.getContext().pImpl->Exprs.create<RealLiteralExpr>(type, value);
}

ExprRef<BvLiteralExpr> BvLiteralExpr::Get(BvType& type, const llvm::APInt& value)
{
    assert(type.getWidth() == value.getBitWidth() && "Bit width of type and value must match!");

    auto& pImpl = type.getContext().pImpl;

    return pImpl->Exprs.create<BvLiteralExpr>(type, value);
}

ExprRef<FloatLiteralExpr> FloatLiteralExpr::Get(FloatType& type, const llvm::APFloat& value)
{
    assert(llvm::APFloat::semanticsSizeInBits(value.getSemantics()) == type.getPrecision());

    return type.getContext().pImpl->Exprs.create<FloatLiteralExpr>(type, value);
}

void UndefExpr::print(llvm::raw_ostream& os) const {
    os << "undef";
}

void BoolLiteralExpr::print(llvm::raw_ostream& os) const {
    os << (mValue ? "True" : "False");
}

void IntLiteralExpr::print(llvm::raw_ostream& os) const {
    os << mValue;
}

void RealLiteralExpr::print(llvm::raw_ostream& os) const {
    os << mValue.numerator() << "/" << mValue.denominator();
}

void BvLiteralExpr::print(llvm::raw_ostream& os) const {
    os << mValue;
}

void FloatLiteralExpr::print(llvm::raw_ostream& os) const
{
    llvm::SmallVector<char, 16> buffer;
    mValue.toString(buffer);
    os << buffer;
}

ExprRef<LiteralExpr> gazer::LiteralFromLLVMConst(GazerContext& context, llvm::ConstantData* value, bool i1AsBool)
{
    if (auto ci = llvm::dyn_cast<llvm::ConstantInt>(value)) {
        unsigned width = ci->getType()->getIntegerBitWidth();
        if (width == 1 && i1AsBool) {
            return BoolLiteralExpr::Get(BoolType::Get(context), ci->isZero() ? false : true);
        }

        return BvLiteralExpr::Get(BvType::Get(context, width), ci->getValue());
    }
    
    if (auto cfp = llvm::dyn_cast<llvm::ConstantFP>(value)) {
        auto fltTy = cfp->getType();
        FloatType::FloatPrecision precision;
        if (fltTy->isHalfTy()) {
            precision = FloatType::Half;
        } else if (fltTy->isFloatTy()) {
            precision = FloatType::Single;
        } else if (fltTy->isDoubleTy()) {
            precision = FloatType::Double;
        } else if (fltTy->isFP128Ty()) {
            precision = FloatType::Quad;
        } else {
            assert(false && "Unsupported floating-point type.");
        }

        return FloatLiteralExpr::Get(FloatType::Get(context, precision), cfp->getValueAPF());
    }

    // TODO: We do not support undefs here.
    assert(false && "Unsupported LLVM constant value.");
}

