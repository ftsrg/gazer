#include "gazer/Core/LiteralExpr.h"
#include "gazer/Support/DenseMapKeyInfo.h"

#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/DenseMap.h>

#include <map>

using namespace gazer;

ExprRef<UndefExpr> UndefExpr::Get(const Type& type)
{
    static std::map<const Type*, ExprRef<UndefExpr>> UndefMap;
    const Type* pType = &type;

    auto result = UndefMap.find(pType);
    if (result == UndefMap.end()) {
        auto ptr = ExprRef<UndefExpr>(new UndefExpr(type));
        UndefMap[pType] = ptr;

        return ptr;
    }

    return result->second;
}

ExprRef<BoolLiteralExpr> BoolLiteralExpr::getTrue()
{
    static auto expr = ExprRef<BoolLiteralExpr>(new BoolLiteralExpr(true));

    return expr;
}

ExprRef<BoolLiteralExpr> BoolLiteralExpr::getFalse()
{
    static auto expr = ExprRef<BoolLiteralExpr>(new BoolLiteralExpr(false));

    return expr;
}

ExprRef<IntLiteralExpr> IntLiteralExpr::get(IntType& type, int64_t value)
{
    static llvm::DenseMap<int64_t, ExprRef<IntLiteralExpr>> Exprs;

    auto result = Exprs.find(value);
    if (result == Exprs.end()) {
        auto ptr = ExprRef<IntLiteralExpr>(new IntLiteralExpr(type, value));
        Exprs[value] = ptr;

        return ptr;
    }

    return result->second;
}

ExprRef<BvLiteralExpr> BvLiteralExpr::Get(llvm::APInt value)
{
    static llvm::DenseMap<llvm::APInt, ExprRef<BvLiteralExpr>, DenseMapAPIntKeyInfo> Exprs;
    static ExprRef<BvLiteralExpr> Int1True = 
        ExprRef<BvLiteralExpr>(new BvLiteralExpr(
            BvType::get(1), llvm::APInt(1, 1)
        ));
    static ExprRef<BvLiteralExpr> Int1False = 
        ExprRef<BvLiteralExpr>(new BvLiteralExpr(
            BvType::get(1), llvm::APInt(1, 0)
        ));

    // Currently our DenseMapAPIntKeyInfo does not allow 1-bit wide APInts as keys.
    // This workaround makes sure that we can also construct those as well.
    if (LLVM_UNLIKELY(value.getBitWidth() == 1)) {
        return value.getBoolValue() ? Int1True : Int1False;
    }

    auto result = Exprs.find(value);
    if (result == Exprs.end()) {
        auto ptr = ExprRef<BvLiteralExpr>(new BvLiteralExpr(
            BvType::get(value.getBitWidth()),
            value
        ));
        Exprs[value] = ptr;

        return ptr;
    }

    return result->second;
}

ExprRef<FloatLiteralExpr> FloatLiteralExpr::get(const FloatType& type, const llvm::APFloat& value)
{
    static llvm::DenseMap<llvm::APFloat, ExprRef<FloatLiteralExpr>, DenseMapAPFloatKeyInfo> Exprs;
    
    auto result = Exprs.find(value);
    if (result == Exprs.end()) {
        auto ptr = ExprRef<FloatLiteralExpr>(new FloatLiteralExpr(type, value));
        Exprs[value] = ptr;

        return ptr;
    }

    return result->second;
}

ExprRef<FloatLiteralExpr> FloatLiteralExpr::get(FloatType::FloatPrecision precision, const llvm::APFloat& value)
{
    return FloatLiteralExpr::get(FloatType::get(precision), value);
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


ExprRef<LiteralExpr> gazer::LiteralFromLLVMConst(llvm::ConstantData* value, bool i1AsBool)
{
    if (auto ci = llvm::dyn_cast<llvm::ConstantInt>(value)) {
        unsigned width = ci->getType()->getIntegerBitWidth();
        if (width == 1 && i1AsBool) {
            return BoolLiteralExpr::Get(ci->isZero() ? false : true);
        }

        return BvLiteralExpr::Get(ci->getValue());
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

        return FloatLiteralExpr::get(precision, cfp->getValueAPF());
    }

    // TODO: We do not support undefs here.
    assert(false && "Unsupported LLVM constant value.");
}

