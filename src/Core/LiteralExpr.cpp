#include "gazer/Core/LiteralExpr.h"
#include "gazer/Support/DenseMapKeyInfo.h"

#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Constants.h>
#include <llvm/ADT/DenseMap.h>

#include <map>

using namespace gazer;

std::shared_ptr<UndefExpr> UndefExpr::Get(const Type& type)
{
    static std::map<const Type*, std::shared_ptr<UndefExpr>> UndefMap;
    const Type* pType = &type;

    auto result = UndefMap.find(pType);
    if (result == UndefMap.end()) {
        auto ptr = std::shared_ptr<UndefExpr>(new UndefExpr(type));
        UndefMap[pType] = ptr;

        return ptr;
    }

    return result->second;
}

std::shared_ptr<BoolLiteralExpr> BoolLiteralExpr::getTrue()
{
    static auto expr = std::shared_ptr<BoolLiteralExpr>(new BoolLiteralExpr(true));

    return expr;
}

std::shared_ptr<BoolLiteralExpr> BoolLiteralExpr::getFalse()
{
    static auto expr = std::shared_ptr<BoolLiteralExpr>(new BoolLiteralExpr(false));

    return expr;
}

std::shared_ptr<IntLiteralExpr> IntLiteralExpr::get(IntType& type, int64_t value)
{
    static llvm::DenseMap<int64_t, std::shared_ptr<IntLiteralExpr>> Exprs;

    auto result = Exprs.find(value);
    if (result == Exprs.end()) {
        auto ptr = std::shared_ptr<IntLiteralExpr>(new IntLiteralExpr(type, value));
        Exprs[value] = ptr;

        return ptr;
    }

    return result->second;
}

std::shared_ptr<BvLiteralExpr> BvLiteralExpr::Get(llvm::APInt value)
{
    static llvm::DenseMap<llvm::APInt, std::shared_ptr<BvLiteralExpr>, DenseMapAPIntKeyInfo> Exprs;
    static std::shared_ptr<BvLiteralExpr> Int1True = 
        std::shared_ptr<BvLiteralExpr>(new BvLiteralExpr(
            BvType::get(1), llvm::APInt(1, 1)
        ));
    static std::shared_ptr<BvLiteralExpr> Int1False = 
        std::shared_ptr<BvLiteralExpr>(new BvLiteralExpr(
            BvType::get(1), llvm::APInt(1, 0)
        ));

    // Currently our DenseMapAPIntKeyInfo does not allow 1-bit wide APInts as keys.
    // This workaround makes sure that we can also construct those as well.
    if (LLVM_UNLIKELY(value.getBitWidth() == 1)) {
        return value.getBoolValue() ? Int1True : Int1False;
    }

    auto result = Exprs.find(value);
    if (result == Exprs.end()) {
        auto ptr = std::shared_ptr<BvLiteralExpr>(new BvLiteralExpr(
            BvType::get(value.getBitWidth()),
            value
        ));
        Exprs[value] = ptr;

        return ptr;
    }

    return result->second;
}

std::shared_ptr<FloatLiteralExpr> FloatLiteralExpr::get(const FloatType& type, const llvm::APFloat& value)
{
    static llvm::DenseMap<llvm::APFloat, std::shared_ptr<FloatLiteralExpr>, DenseMapAPFloatKeyInfo> Exprs;
    
    auto result = Exprs.find(value);
    if (result == Exprs.end()) {
        auto ptr = std::shared_ptr<FloatLiteralExpr>(new FloatLiteralExpr(type, value));
        Exprs[value] = ptr;

        return ptr;
    }

    return result->second;
}

std::shared_ptr<FloatLiteralExpr> FloatLiteralExpr::get(FloatType::FloatPrecision precision, const llvm::APFloat& value)
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

std::shared_ptr<LiteralExpr> gazer::LiteralFromLLVMConst(llvm::ConstantData* value, bool i1AsBool)
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

