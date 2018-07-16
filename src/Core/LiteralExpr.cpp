#include "gazer/Core/LiteralExpr.h"

#include <llvm/Support/raw_ostream.h>

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

void UndefExpr::print(llvm::raw_ostream& os) const {
    os << "undef";
}

void BoolLiteralExpr::print(llvm::raw_ostream& os) const {
    os << (mValue ? "True" : "False");
}

void IntLiteralExpr::print(llvm::raw_ostream& os) const {
    os << mValue;
}
