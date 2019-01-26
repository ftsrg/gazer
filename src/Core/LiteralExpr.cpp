#include "gazer/Core/LiteralExpr.h"
#include "gazer/Support/DenseMapKeyInfo.h"

#include "GazerContextImpl.h"

#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/DenseMap.h>

#include <map>

using namespace gazer;

ExprRef<UndefExpr> UndefExpr::Get(Type& type)
{
    auto& pImpl = type.getContext().pImpl;

    auto result = pImpl->Undefs.find(&type);
    if (result == pImpl->Undefs.end()) {
        auto ptr = ExprRef<UndefExpr>(new UndefExpr(type));
        pImpl->Undefs[&type] = ptr;

        return ptr;
    }

    return result->second;
}

ExprRef<BoolLiteralExpr> BoolLiteralExpr::True(BoolType& type) {
    return type.getContext().pImpl->TrueLit;
}

ExprRef<BoolLiteralExpr> BoolLiteralExpr::False(BoolType& type) {
    return type.getContext().pImpl->FalseLit;
}

ExprRef<IntLiteralExpr> IntLiteralExpr::Get(IntType& type, int64_t value)
{
    llvm_unreachable("Int literals are not supported yet.");
}

ExprRef<BvLiteralExpr> BvLiteralExpr::Get(BvType& type, llvm::APInt value)
{
    assert(type.getWidth() == value.getBitWidth() && "Bit width of type and value must match!");

    auto& pImpl = type.getContext().pImpl;

    // Currently our DenseMapAPIntKeyInfo does not allow 1-bit wide APInts as keys.
    // This workaround makes sure that we can also construct those as well.
    if (LLVM_UNLIKELY(value.getBitWidth() == 1)) {
        return value.getBoolValue() ? pImpl->Bv1True : pImpl->Bv1False;
    }

    auto result = pImpl->BvLiterals.find(value);
    if (result == pImpl->BvLiterals.end()) {
        auto ptr = ExprRef<BvLiteralExpr>(new BvLiteralExpr(type, value));
        pImpl->BvLiterals[value] = ptr;

        return ptr;
    }

    return result->second;
}

ExprRef<FloatLiteralExpr> FloatLiteralExpr::Get(FloatType& type, const llvm::APFloat& value)
{
    assert(llvm::APFloat::semanticsPrecision(value.getSemantics()) == type.getPrecision());

    auto& pImpl = type.getContext().pImpl;

    auto result = pImpl->FloatLiterals.find(value);
    if (result == pImpl->FloatLiterals.end()) {
        auto ptr = ExprRef<FloatLiteralExpr>(new FloatLiteralExpr(type, value));
        pImpl->FloatLiterals[value] = ptr;

        return ptr;
    }

    return result->second;
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

void BvLiteralExpr::print(llvm::raw_ostream& os) const {
    os << mValue;
}

void FloatLiteralExpr::print(llvm::raw_ostream& os) const
{
    llvm::SmallVector<char, 16> buffer;
    mValue.toString(buffer);
    os << buffer;
}

template<class ExprTy>
static bool equals_helper(const ExprTy* left, const LiteralExpr& right)
{
    if (const ExprTy* lit = llvm::dyn_cast<ExprTy>(&right)) {
        return left->getValue() == lit->getValue();
    }

    return false;
}

#define LITERAL_EQUALS(LITERALCLASS)                                   \
bool LITERALCLASS::equals(const LiteralExpr& other) const  {           \
    return equals_helper(this, other);                                 \
}

LITERAL_EQUALS(BoolLiteralExpr)
LITERAL_EQUALS(BvLiteralExpr)
LITERAL_EQUALS(IntLiteralExpr)

#undef LITERAL_EQUALS

bool FloatLiteralExpr::equals(const LiteralExpr& other) const
{
    if (auto lit = llvm::dyn_cast<FloatLiteralExpr>(&other)) {
        return this->getValue().bitwiseIsEqual(lit->getValue());
    }

    return false;
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

