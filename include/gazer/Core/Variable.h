#ifndef _GAZER_CORE_VARIABLE_H
#define _GAZER_CORE_VARIABLE_H

#include "gazer/Core/Expr.h"

#include <string>
#include <memory>

namespace gazer
{

class VariableRefExpr;

class Variable final
{
public:
    Variable(std::string name, const Type& type);

    Variable(const Variable&) = delete;
    Variable& operator=(const Variable&) = delete;

    const Type& getType() const { return mType; }
    std::string getName() const { return mName; }
    std::shared_ptr<VariableRefExpr> getRefExpr() const { return mExpr; }

private:
    std::string mName;
    const Type& mType;
    std::shared_ptr<VariableRefExpr> mExpr;
};

class VariableRefExpr final : public Expr
{
    friend class Variable;
private:
    VariableRefExpr(const Variable& variable)
        : Expr(Expr::VarRef, variable.getType()), mVariable(variable)
    {}
public:
    const Variable& getVariable() const { return mVariable; }

private:
    const Variable& mVariable;
};

}

#endif
