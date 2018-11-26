#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Variable.h"

#include <llvm/ADT/DenseMap.h>

using namespace gazer;
using llvm::dyn_cast;

namespace
{

class FoldingExprBuilder : public ExprBuilder
{
public:
    FoldingExprBuilder()
        : ExprBuilder()
    {}

    ExprPtr Not(const ExprPtr& op) override
    {
        if (auto boolLit = llvm::dyn_cast<BoolLiteralExpr>(op.get())) {
            return BoolLiteralExpr::Get(!boolLit->getValue());
        }

        return NotExpr::Create(op);
    }

    ExprPtr ZExt(const ExprPtr& op, const BvType& type) override {
        return ZExtExpr::Create(op, type);
    }
    ExprPtr SExt(const ExprPtr& op, const BvType& type) override {
        return SExtExpr::Create(op, type);
    }
    ExprPtr Trunc(const ExprPtr& op, const BvType& type) override {
        return ExtractExpr::Create(op, 0, type.getWidth());
    }
    ExprPtr Extract(const ExprPtr& op, unsigned offset, unsigned width) override {
        return ExtractExpr::Create(op, offset, width);
    }

    ExprPtr Add(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue() + rhsLit->getValue());
            }
        }

        return AddExpr::Create(left, right);
    }

    ExprPtr Sub(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue() - rhsLit->getValue());
            }
        }

        return SubExpr::Create(left, right);
    }

    ExprPtr Mul(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue() * rhsLit->getValue());
            }
        }

        return MulExpr::Create(left, right);
    }

    ExprPtr SDiv(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue().sdiv(rhsLit->getValue()));
            }
        }

        return SDivExpr::Create(left, right);
    }

    ExprPtr UDiv(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue().udiv(rhsLit->getValue()));
            }
        }

        return UDivExpr::Create(left, right);
    }

    ExprPtr SRem(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue().srem(rhsLit->getValue()));
            }
        }

        return SRemExpr::Create(left, right);
    }

    ExprPtr URem(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue().urem(rhsLit->getValue()));
            }
        }

        return URemExpr::Create(left, right);
    }

    ExprPtr Shl(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue().shl(rhsLit->getValue()));
            }
        }

        return ShlExpr::Create(left, right);
    }

    ExprPtr LShr(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue().lshr(rhsLit->getValue()));
            }
        }
        return LShrExpr::Create(left, right);
    }

    ExprPtr AShr(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue().ashr(rhsLit->getValue()));
            }
        }
        return AShrExpr::Create(left, right);
    }

    ExprPtr BAnd(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue() & rhsLit->getValue());
            }
        }
        return BAndExpr::Create(left, right);
    }

    ExprPtr BOr(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue() | rhsLit->getValue());
            }
        }
        return BOrExpr::Create(left, right);
    }

    ExprPtr BXor(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return this->BvLit(lhsLit->getValue() ^ rhsLit->getValue());
            }
        }

        return BXorExpr::Create(left, right);
    }

    ExprPtr And(const ExprVector& vector) override
    {
        ExprVector newOps;

        for (const ExprPtr& op : vector) {
            if (op->getKind() == Expr::Literal) {
                auto lit = llvm::dyn_cast<BoolLiteralExpr>(op.get());
                if (lit->getValue() == false) {
                    return this->False();
                } else {
                    // We are not adding unnecessary true literals
                }
            } else if (op->getKind() == Expr::And) {
                // For AndExpr operands, we try to flatten the expression
                auto andExpr = llvm::dyn_cast<AndExpr>(op.get());    
                newOps.insert(newOps.end(), andExpr->op_begin(), andExpr->op_end());
            } else {
                newOps.push_back(op);
            }
        }

        if (newOps.size() == 0) {
            // If we eliminated all operands
            return this->True();
        } else if (newOps.size() == 1) {
            return *newOps.begin();
        }
        
        // Try some optimizations for the binary case
        if (newOps.size() == 2) {
            auto left = llvm::dyn_cast<OrExpr>(newOps[0].get());
            auto right = llvm::dyn_cast<OrExpr>(newOps[1].get());
            if (left != nullptr && right != nullptr) {
                // (F1 | F2) & (F1 | F3) -> F1 | (F2 & F3)
                ExprVector intersect;   // F1
                std::set_intersection(
                    left->op_begin(), left->op_end(),
                    right->op_begin(), right->op_end(),
                    std::back_inserter(intersect)
                );

                if (!intersect.empty()) {
                    ExprVector newLeft;     // F2
                    ExprVector newRight;    // F3
                    std::set_difference(
                        left->op_begin(), left->op_end(),
                        intersect.begin(), intersect.end(),
                        std::back_inserter(newLeft)
                    );
                    std::set_difference(
                        right->op_begin(), right->op_end(),
                        intersect.begin(), intersect.end(),
                        std::back_inserter(newRight)
                    );

                    return this->Or({
                        OrExpr::Create(intersect.begin(), intersect.end()),
                        this->And({
                            OrExpr::Create(newLeft.begin(), newLeft.end()),
                            OrExpr::Create(newRight.begin(), newRight.end())
                        })
                    });
                }
            }
        }

        return AndExpr::Create(newOps.begin(), newOps.end());
    }

    ExprPtr Or(const ExprVector& vector) override
    {
        ExprVector newOps;

        for (const ExprPtr& op : vector) {
            if (op->getKind() == Expr::Literal) {
                auto lit = llvm::dyn_cast<BoolLiteralExpr>(op.get());
                if (lit->getValue() == true) {
                    return this->True();
                } else {
                    // We are not adding unnecessary false literals
                }
            } else if (op->getKind() == Expr::Or) {
                // For OrExpr operands, we try to flatten the expression
                auto orExpr = llvm::dyn_cast<OrExpr>(op.get());    
                newOps.insert(newOps.end(), orExpr->op_begin(), orExpr->op_end());
            } else {
                newOps.push_back(op);
            }
        }

        if (newOps.size() == 0) {
            // If we eliminated all operands
            return this->False();
        } else if (newOps.size() == 1) {
            return *newOps.begin();
        }

        // Try some optimizations for the binary case
        if (newOps.size() == 2) {
            auto left = llvm::dyn_cast<AndExpr>(newOps[0].get());
            auto right = llvm::dyn_cast<AndExpr>(newOps[1].get());
            if (left != nullptr && right != nullptr) {
                // (F1 & F2) | (F1 & F3) -> F1 & (F2 | F3)
                ExprVector intersect;
                std::set_intersection(
                    left->op_begin(), left->op_end(),
                    right->op_begin(), right->op_end(),
                    std::back_inserter(intersect)
                );

                if (!intersect.empty()) {
                    ExprVector newLeft;
                    ExprVector newRight;
                    std::set_difference(
                        left->op_begin(), left->op_end(),
                        intersect.begin(), intersect.end(),
                        std::back_inserter(newLeft)
                    );
                    std::set_difference(
                        right->op_begin(), right->op_end(),
                        intersect.begin(), intersect.end(),
                        std::back_inserter(newRight)
                    );

                    return this->And({
                        AndExpr::Create(intersect.begin(), intersect.end()),
                        this->Or({
                            AndExpr::Create(newLeft.begin(), newLeft.end()),
                            AndExpr::Create(newRight.begin(), newRight.end())
                        })
                    });
                }
            }
        }
       
        return OrExpr::Create(newOps.begin(), newOps.end());
    }

    ExprPtr Xor(const ExprPtr& left, const ExprPtr& right) override
    {
        if (left == this->True()) {
            return this->Not(right);
        } else if (right == this->True()) {
            return this->Not(left);
        } else if (left == this->False()) {
            return right;
        } else if (right == this->False()) {
            return left;
        }

        return XorExpr::Create(left, right);
    }

    ExprPtr Eq(const ExprPtr& left, const ExprPtr& right) override
    {
        if (left == right) {
            return this->True();
        }

        auto& opTy = left->getType();
        if (opTy.isBoolType()) {
            // true == X -> X
            // false == X -> not X
            /*
            if (auto lb = dyn_cast<BoolLiteralExpr>(left.get())) {
                return lb->isTrue() ? right : this->Not(right);
            } else if (auto rb = dyn_cast<BoolLiteralExpr>(right.get())) {
                return rb->isTrue() ? left : this->Not(left);
            } */
        } else if (auto lhsLit = dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return BoolLiteralExpr::Get(lhsLit->getValue() == rhsLit->getValue()); 
            }
        } else if (left->getKind() == Expr::VarRef && right->getKind() == Expr::VarRef) {
            auto lhsVar = &(llvm::dyn_cast<VarRefExpr>(left.get())->getVariable());
            auto rhsVar = &(llvm::dyn_cast<VarRefExpr>(right.get())->getVariable());

            if (lhsVar == rhsVar) {
                return this->True();
            }
        }

        return EqExpr::Create(left, right);
    }

    ExprPtr NotEq(const ExprPtr& left, const ExprPtr& right) override
    {
        if (left == right) {
            return this->False();
        }

        auto& opTy = left->getType();
        if (opTy.isBoolType()) {
            // false != X -> X
            // true == X -> not X
            /*
            if (auto lb = dyn_cast<BoolLiteralExpr>(left.get())) {
                return lb->isFalse() ? right : this->Not(right);
            } else if (auto rb = dyn_cast<BoolLiteralExpr>(right.get())) {
                return rb->isFalse() ? left : this->Not(left);
            }*/
        } else if (auto lhsLit = dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return BoolLiteralExpr::Get(lhsLit->getValue() != rhsLit->getValue()); 
            }
        } else if (left->getKind() == Expr::VarRef && right->getKind() == Expr::VarRef) {
            auto lhsVar = &(llvm::dyn_cast<VarRefExpr>(left.get())->getVariable());
            auto rhsVar = &(llvm::dyn_cast<VarRefExpr>(right.get())->getVariable());

            if (lhsVar == rhsVar) {
                return this->False();
            }
        }
        
        return NotEqExpr::Create(left, right);
    }

    ExprPtr SLt(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return BoolLiteralExpr::Get(lhsLit->getValue().slt(rhsLit->getValue()));
            }
        }

        return SLtExpr::Create(left, right);
    }

    ExprPtr SLtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return BoolLiteralExpr::Get(lhsLit->getValue().sle(rhsLit->getValue()));
            }
        }


        return SLtEqExpr::Create(left, right);
    }

    ExprPtr SGt(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return BoolLiteralExpr::Get(lhsLit->getValue().sgt(rhsLit->getValue()));
            }
        }

        return SGtExpr::Create(left, right);
    }

    ExprPtr SGtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return BoolLiteralExpr::Get(lhsLit->getValue().sge(rhsLit->getValue()));
            }
        }


        return SGtEqExpr::Create(left, right);
    }

    ExprPtr ULt(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return BoolLiteralExpr::Get(lhsLit->getValue().ult(rhsLit->getValue()));
            }
        }

        return ULtExpr::Create(left, right);
    }

    ExprPtr ULtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return BoolLiteralExpr::Get(lhsLit->getValue().ule(rhsLit->getValue()));
            }
        }

        return ULtEqExpr::Create(left, right);
    }

    ExprPtr UGt(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return BoolLiteralExpr::Get(lhsLit->getValue().ugt(rhsLit->getValue()));
            }
        }

        return UGtExpr::Create(left, right);
    }

    ExprPtr UGtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left.get())) {
            if (auto rhsLit = dyn_cast<BvLiteralExpr>(right.get())) {
                return BoolLiteralExpr::Get(lhsLit->getValue().uge(rhsLit->getValue()));
            }
        }

        return UGtEqExpr::Create(left, right);
    }

    ExprPtr FIsNan(const ExprPtr& op) override {
        if (op->getKind() == Expr::Literal) {
            auto fltLit = llvm::dyn_cast<FloatLiteralExpr>(op.get());
            return BoolLiteralExpr::Get(fltLit->getValue().isNaN());
        }

        return FIsNanExpr::Create(op);
    }

    ExprPtr FIsInf(const ExprPtr& op) override {
        if (op->getKind() == Expr::Literal) {
            auto fltLit = llvm::dyn_cast<FloatLiteralExpr>(op.get());
            return BoolLiteralExpr::Get(fltLit->getValue().isInfinity());
        }

        return FIsInfExpr::Create(op);
    }
    
    ExprPtr FAdd(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override {
        return FAddExpr::Create(left, right, rm);
    }
    ExprPtr FSub(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override {
        return FSubExpr::Create(left, right, rm);
    }
    ExprPtr FMul(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override {
        return FMulExpr::Create(left, right, rm);
    }
    ExprPtr FDiv(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override {
        if (left->getKind() == Expr::Literal && right->getKind() == Expr::Literal) {
            auto fltLeft  = llvm::dyn_cast<FloatLiteralExpr>(left.get());
            auto fltRight = llvm::dyn_cast<FloatLiteralExpr>(right.get());

            llvm::APFloat result(fltLeft->getValue());
            result.divide(fltRight->getValue(), rm);

            return FloatLiteralExpr::get(
                *llvm::dyn_cast<FloatType>(&left->getType()), result
            );
        }

        return FDivExpr::Create(left, right, rm);
    }
    
    ExprPtr FEq(const ExprPtr& left, const ExprPtr& right) override {
        return FEqExpr::Create(left, right);
    }
    ExprPtr FGt(const ExprPtr& left, const ExprPtr& right) override {
        return FGtExpr::Create(left, right);
    }
    ExprPtr FGtEq(const ExprPtr& left, const ExprPtr& right) override {
        return FGtEqExpr::Create(left, right);
    }
    ExprPtr FLt(const ExprPtr& left, const ExprPtr& right) override {
        return FLtExpr::Create(left, right);
    }
    ExprPtr FLtEq(const ExprPtr& left, const ExprPtr& right) override {
        return FLtEqExpr::Create(left, right);
    }

    ExprPtr Select(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze) override
    {
        if (condition->getKind() == Expr::Literal) {
            auto lit = llvm::dyn_cast<BoolLiteralExpr>(condition.get());
            if (lit->getValue() == true) {
                return then;
            } else {
                return elze;
            }
        }

        return SelectExpr::Create(condition, then, elze);
    }
};

} // end anonymous namespace

std::unique_ptr<ExprBuilder> gazer::CreateFoldingExprBuilder() {
    return std::unique_ptr<ExprBuilder>(new FoldingExprBuilder());
}