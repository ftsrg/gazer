#include "gazer/Core/Variable.h"

using namespace gazer;

Variable::Variable(std::string name, const Type& type)
    : mName(name), mType(type)
{
    mExpr = std::shared_ptr<VarRefExpr>(new VarRefExpr(*this));
}

void VarRefExpr::print(std::ostream& os) const {
    os << mVariable.getName();
}
