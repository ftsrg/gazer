#include "gazer/Core/LiteralExpr.h"

#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Constants.h>

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

std::shared_ptr<IntLiteralExpr> IntLiteralExpr::get(IntType& type, llvm::APInt value)
{
    //static std::map<int, std::shared_ptr<IntLiteralExpr>> exprs;
    
    //auto result = exprs.find(value);
    //if (result == exprs.end()) {
    //    auto ptr = std::shared_ptr<IntLiteralExpr>(new IntLiteralExpr(value));
    //    exprs[value] = ptr;

    //    return ptr;
    //}

    //return result->second;

    return std::shared_ptr<IntLiteralExpr>(new IntLiteralExpr(type, value));
}

std::shared_ptr<FloatLiteralExpr> FloatLiteralExpr::get(const FloatType& type, const llvm::APFloat& value)
{
    return std::shared_ptr<FloatLiteralExpr>(new FloatLiteralExpr(type, value));
}

std::shared_ptr<FloatLiteralExpr> FloatLiteralExpr::get(FloatType::FloatPrecision precision, const llvm::APFloat& value)
{
    return std::shared_ptr<FloatLiteralExpr>(new FloatLiteralExpr(
        *FloatType::get(precision), value
    ));
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

void FloatLiteralExpr::print(llvm::raw_ostream& os) const {
    llvm::SmallVector<char, 16> buffer;
    mValue.toString(buffer);
    os << buffer;
}

std::shared_ptr<LiteralExpr> gazer::LiteralFromLLVMConst(llvm::ConstantData* value, bool i1IsBool)
{
    if (auto ci = llvm::dyn_cast<llvm::ConstantInt>(value)) {
        unsigned width = ci->getType()->getIntegerBitWidth();
        if (width == 1 && i1IsBool) {
            return BoolLiteralExpr::Get(ci->isZero() ? false : true);
        }

        return IntLiteralExpr::get(*IntType::get(width), ci->getValue());
    } else if (auto cfp = llvm::dyn_cast<llvm::ConstantFP>(value)) {
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

