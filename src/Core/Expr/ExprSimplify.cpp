#include "gazer/Core/ExprVisitor.h"
#include "gazer/Core/Valuation.h"
//#include "gazer/Core/Expr/Matcher.h"

#include <llvm/ADT/DenseMap.h>
#include <stack>


using namespace gazer;

namespace
{

class ExprSimplifyVisitor final : public ExprVisitor<ExprPtr>
{
public:
    virtual ExprPtr visitExpr(const ExprPtr& expr) override { return expr; }

    virtual ExprPtr visitVarRef(const std::shared_ptr<VarRefExpr>& expr) {}

private:
    std::stack<Valuation> mValuations;
};

}