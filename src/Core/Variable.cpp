#include "gazer/Core/Variable.h"

using namespace gazer;

Variable::Variable(std::string name, const Type& type)
    : mName(name), mType(type)
{
    mExpr = std::shared_ptr<VariableRefExpr>(new VariableRefExpr(*this));
}