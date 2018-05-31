#include "gazer/Core/LiteralExpr.h"

#include <map>

using namespace gazer;

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

void BoolLiteralExpr::print(std::ostream& os) const {
    os << std::boolalpha << mValue;
}

std::shared_ptr<IntLiteralExpr> IntLiteralExpr::get(IntType& type, uint64_t value)
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

void IntLiteralExpr::print(std::ostream& os) const {
    os << mValue;
}
