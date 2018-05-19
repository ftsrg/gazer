#include "gazer/Core/Expr.h"

#include <llvm/Support/raw_ostream.h>

using namespace gazer;

std::ostream& gazer::operator<<(std::ostream& os, const Expr& expr)
{
    expr.print(os);
    return os;
}
