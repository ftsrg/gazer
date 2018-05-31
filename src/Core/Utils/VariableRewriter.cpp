#include "gazer/Core/Utils/CfaUtils.h"

#include <llvm/ADT/DenseMap.h>

using namespace gazer;

namespace
{

std::string getIndexedVariableName(std::string name, unsigned index) {
    return name + "_" + std::to_string(index);
}

class VariableIndexVisitor : public ExprVisitor<ExprPtr>
{
    static constexpr unsigned DefaultIndex = 0;
public:
    VariableIndexVisitor(VariableIndex* vi)
        : mVariableIndex(vi)
    {}
protected:
    // Nullary
    virtual ExprPtr visitLiteral(const std::shared_ptr<LiteralExpr>& expr) override {
        return expr;
    }

    virtual ExprPtr visitVarRef(const std::shared_ptr<VarRefExpr>& expr) override {
        const Variable* variable = &expr->getVariable();
        auto current = mVariableIndex->current(variable);

        return current->getRefExpr();
    }

    virtual ExprPtr visitNonNullary(const std::shared_ptr<NonNullaryExpr>& expr) override {
        std::vector<ExprPtr> ops(expr->getNumOperands());
        for (size_t i = 0; i < expr->getNumOperands(); ++i) {
            ops[i] = this->visit(expr->getOperand(i));
        }

        auto ptr = expr->with(ops.begin(), ops.end());
        if (ptr == expr.get()) { // No changes were made
            return expr;
        }

        return std::shared_ptr<Expr>(ptr);
    }
    
    virtual ExprPtr visitExpr(const ExprPtr& expr) override {
        llvm_unreachable("Unhandled expression kind in VariableRewriter.");
        return nullptr;
    }

private:
    VariableIndex* mVariableIndex;
};

}

VariableIndex::VariableIndex(SymbolTable& symbols)
{
    for (auto& entry : symbols) {
        auto& variable = entry.second;
        mMap[variable.get()] = DefaultIndex;

        auto name = getIndexedVariableName(variable->getName(), DefaultIndex);
        mSymbols.create(name, variable->getType());
    }
}

Variable* VariableIndex::increment(const Variable* variable)
{
    // try to find the current index
    auto result = mMap.find(variable);
    assert(result != mMap.end() && "Variables should be found in the VarMap.");

    auto name = getIndexedVariableName(variable->getName(), ++(result->second));

    // Create a variable for this new index.
    Variable& newVar = mSymbols.create(name, variable->getType());
    
    return &newVar;
}

Variable* VariableIndex::current(const Variable* variable)
{
    auto result = mMap.find(variable);
    assert(result != mMap.end() && "Variables should be found in the VarMap.");

    auto name = getIndexedVariableName(variable->getName(), result->second);
    return &(mSymbols[name]);
}

std::unique_ptr<ExprVisitor<ExprPtr>> gazer::createVariableRewriteVisitor(VariableIndex* vi) {
    return std::make_unique<VariableIndexVisitor>(vi);
}