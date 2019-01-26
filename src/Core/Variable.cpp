#include "gazer/Core/Variable.h"

#include <llvm/Support/raw_ostream.h>

using namespace gazer;

Variable::Variable(std::string name, Type& type)
    : mName(name), mType(type), mExpr(new VarRefExpr(this))
{
}

bool Variable::operator==(const Variable &other) const
{
    if (&getContext() != &other.getContext()) {
        return false;
    }

    return mName == other.mName;
}

void VarRefExpr::print(llvm::raw_ostream& os) const {
    os << mVariable->getType().getName() << " " << mVariable->getName();

}

llvm::raw_ostream& gazer::operator<<(llvm::raw_ostream& os, const Variable& variable)
{
    os << variable.getType() << " " << variable.getName();
    return os;
}
