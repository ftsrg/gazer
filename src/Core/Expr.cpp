#include "gazer/Core/Expr.h"
#include "gazer/Core/ExprTypes.h"

#include <llvm/Support/raw_ostream.h>

using namespace gazer;

std::string Expr::getKindName(ExprKind kind)
{
    static const char* const Names[] = {
        "Undef",
        "Literal",
        "VarRef",
        "Not",
        "ZExt",
        "SExt",
        "Trunc",
        "Add",
        "Sub",
        "Mul",
        "SDiv",
        "UDiv",
        "SRem",
        "URem",
        "Shl",
        "LShr",
        "AShr",
        "BAnd",
        "BOr",
        "BXor",
        "And",
        "Or",
        "Xor",
        "Eq",
        "NotEq",
        "SLt",
        "SLtEq",
        "SGt",
        "SGtEq",
        "ULt",
        "ULtEq",
        "UGt",
        "UGtEq",
        "FIsNan",
        "FIsInf",
        "FAdd",
        "FSub",
        "FMul",
        "FDiv",
        "FEq",
        "FGt",
        "FGtEq",
        "FLt",
        "FLtEq",
        "Select",
        "ArrayRead",
        "ArrayWrite",
    };

    static_assert(
        (sizeof(Names) / sizeof(Names[0])) == LastExprKind + 1,
        "Missing ExprKind in Expr::print()"
    );

    if (Expr::FirstExprKind <= kind && kind <= Expr::LastExprKind) {
        return Names[kind];
    }

    llvm_unreachable("Invalid expression kind.");
}

void NonNullaryExpr::print(llvm::raw_ostream& os) const
{
    size_t i = 0;
    os << getType().getName() << " " << Expr::getKindName(getKind()) << "(";
    while (i < getNumOperands() - 1) {
        getOperand(i)->print(os);
        os << ",";
        ++i;
    }

    getOperand(i)->print(os);
    os << ")";
}

ExprPtr NonNullaryExpr::clone(ExprVector ops)
{
    assert(this->getNumOperands() == ops.size()
        && "Operand counts must match for cloning!");
    assert(std::none_of(
        ops.begin(), ops.end(), [](auto& op) { return op == nullptr; }
    ) && "Cannot clone with a nullptr operand!");

    if (std::equal(ops.begin(), ops.end(), this->op_begin())) {
        // The operands are the same, just return the original object
        return make_expr_ref(this);
    }

    // Otherwise perform the cloning operation and return a new ExprRef handle
    return this->cloneImpl(ops);
}

llvm::raw_ostream& gazer::operator<<(llvm::raw_ostream& os, const Expr& expr)
{
    expr.print(os);
    return os;
}

ExprRef<SelectExpr> SelectExpr::Create(ExprPtr condition, ExprPtr then, ExprPtr elze)
{
    assert(then->getType() == elze->getType() && "Select expression operand types must match.");
    assert(condition->getType().isBoolType() && "Select expression condition type must be boolean.");
    return ExprRef<SelectExpr>(new SelectExpr(then->getType(), condition, then, elze));
}

ExprRef<ArrayReadExpr> ArrayReadExpr::Create(
    ExprRef<VarRefExpr> array, ExprPtr index
) {
    assert(array->getType().isArrayType() && "ArrayRead only works on arrays.");
    const ArrayType* arrTy = llvm::cast<ArrayType>(&array->getType());
    assert(arrTy->getIndexType() == index->getType() &&
        "Array index type and index types must match.");

    return ExprRef<ArrayReadExpr>(new ArrayReadExpr(array, index));
}

ExprRef<ArrayWriteExpr> ArrayWriteExpr::Create(
    ExprRef<VarRefExpr> array, ExprPtr index, ExprPtr value
) {
    assert(array->getType().isArrayType() && "ArrayRead only works on arrays.");
    const ArrayType* arrTy = llvm::cast<ArrayType>(&array->getType());
    assert(arrTy->getIndexType() == index->getType() &&
        "Array index type and index types must match.");

    return ExprRef<ArrayWriteExpr>(new ArrayWriteExpr(array, index, value));
}
