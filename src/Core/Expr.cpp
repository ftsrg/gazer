#include "gazer/Core/Expr.h"
#include "gazer/Core/ExprTypes.h"

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/raw_os_ostream.h>

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

std::ostream& gazer::operator<<(std::ostream& os, const Expr& expr)
{
    llvm::raw_os_ostream ros(os);
    expr.print(ros);
    return os;
}

std::shared_ptr<SelectExpr> SelectExpr::Create(ExprPtr condition, ExprPtr then, ExprPtr elze)
{
    assert(then->getType() == elze->getType() && "Select expression operand types must match.");
    assert(condition->getType().isBoolType() && "Select expression condition type must be boolean.");
    return std::shared_ptr<SelectExpr>(new SelectExpr(then->getType(), condition, then, elze));
}

std::shared_ptr<ArrayReadExpr> ArrayReadExpr::Create(
    std::shared_ptr<VarRefExpr> array, ExprPtr index
) {
    assert(array->getType().isArrayType() && "ArrayRead only works on arrays.");
    const ArrayType* arrTy = llvm::cast<ArrayType>(&array->getType());
    assert(arrTy->getIndexType()->equals(&index->getType()) &&
        "Array index type and index types must match.");

    return std::shared_ptr<ArrayReadExpr>(new ArrayReadExpr(array, index));
}

Expr* ArrayReadExpr::withOps(std::vector<ExprPtr> ops) const
{
    assert(ops[0]->getKind() == Expr::VarRef && "Array references must be variables");
    assert(ops[0]->getType() == getType() && "withOps() cannot change type");
    assert(ops[1]->getType() == getIndex()->getType()
        && "Array index type and index types must match.");

    return new ArrayReadExpr(std::static_pointer_cast<VarRefExpr>(ops[0]), ops[1]);
}

std::shared_ptr<ArrayWriteExpr> ArrayWriteExpr::Create(
    std::shared_ptr<VarRefExpr> array, ExprPtr index, ExprPtr value
) {
    assert(array->getType().isArrayType() && "ArrayRead only works on arrays.");
    const ArrayType* arrTy = llvm::cast<ArrayType>(&array->getType());
    assert(arrTy->getIndexType()->equals(&index->getType()) &&
        "Array index type and index types must match.");

    return std::shared_ptr<ArrayWriteExpr>(new ArrayWriteExpr(array, index, value));
}

Expr* ArrayWriteExpr::withOps(std::vector<ExprPtr> ops) const
{    
    assert(ops[0]->getKind() == Expr::VarRef && "Array references must be variables");
    assert(ops[0]->getType() == getType() && "withOps() cannot change type");
    assert(ops[1]->getType() == getIndex()->getType()
        && "Array index type and index types must match.");
    assert(ops[2]->getType() == getElementValue()->getType()
        && "Array element type and element types must match.");

    return new ArrayWriteExpr(
        std::static_pointer_cast<VarRefExpr>(ops[0]),
        ops[1],
        ops[2]
    );
}
