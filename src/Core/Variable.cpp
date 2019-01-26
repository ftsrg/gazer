#include "gazer/Core/Variable.h"

#include <llvm/Support/raw_ostream.h>

using namespace gazer;

Variable::Variable(std::string name, const Type& type)
    : mName(name), mType(type)
{
    mExpr = ExprRef<VarRefExpr>(new VarRefExpr(*this));
}

bool Variable::operator==(const Variable& other) const {
    return other.mName == this->mName;
}

void VarRefExpr::print(llvm::raw_ostream& os) const {
    os << mVariable.getType().getName() << " " << mVariable.getName();

}


llvm::raw_ostream& gazer::operator<<(llvm::raw_ostream& os, const Variable& variable)
{
    os << variable.getType() << " " << variable.getName();
    return os;
}
