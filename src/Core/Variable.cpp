#include "gazer/Core/Variable.h"

#include <llvm/Support/raw_ostream.h>

using namespace gazer;

Variable::Variable(std::string name, const Type& type)
    : mName(name), mType(type)
{
    mExpr = std::shared_ptr<VarRefExpr>(new VarRefExpr(*this));
}

void VarRefExpr::print(llvm::raw_ostream& os) const {
    os << mVariable.getType().getName() << " " << mVariable.getName();
}
