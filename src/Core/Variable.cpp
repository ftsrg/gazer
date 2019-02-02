#include "gazer/Core/Variable.h"
#include "GazerContextImpl.h"

#include <llvm/Support/raw_ostream.h>

using namespace gazer;

Variable::Variable(llvm::StringRef name, Type& type)
    : mName(name), mType(type)
{
    mExpr = type.getContext().pImpl->Exprs.create<VarRefExpr>(this);
}

VarRefExpr::VarRefExpr(Variable* variable)
    : Expr(Expr::VarRef, variable->getType()), mVariable(variable)
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
