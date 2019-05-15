#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Variable.h"
#include "gazer/Core/Expr/Matcher.h"
#include "gazer/Core/Expr/ConstantFolder.h"
#include "gazer/Core/Expr/ExprPropagator.h"

#include <llvm/ADT/DenseMap.h>

using namespace gazer;
using namespace gazer::PatternMatch;

using llvm::dyn_cast;

namespace
{

class FoldingExprBuilder : public ExprBuilder
{
public:
    FoldingExprBuilder(GazerContext& context)
        : ExprBuilder(context)
    {}

    ExprPtr Not(const ExprPtr& op) override
    {
        // Not(Not(X)) --> X
        if (op->getKind() == Expr::Not) {
            return llvm::cast<NotExpr>(op.get())->getOperand();
        }

        ExprPtr e1 = nullptr, e2 = nullptr;

        // Not(Eq(E1, E2)) --> NotEq(E1, E2)
        if (match(op, m_Eq(m_Expr(e1), m_Expr(e2)))) {
            return ConstantFolder::NotEq(e1, e2);
        }

        // Not(NotEq(E1, E2)) --> Eq(E1, E2)
        if (match(op, m_NotEq(m_Expr(e1), m_Expr(e2)))) {
            return ConstantFolder::Eq(e1, e2);
        }

        // Not(LESSTHAN(E1, E2)) --> GREATERTHANEQ(E1, E2)
        if (match(op, m_ULt(m_Expr(e1), m_Expr(e2)))) {
            return ConstantFolder::UGtEq(e1, e2);
        }

        if (match(op, m_SLt(m_Expr(e1), m_Expr(e2)))) {
            return ConstantFolder::SGtEq(e1, e2);
        }

        return ConstantFolder::Not(op);
    }

    ExprPtr ZExt(const ExprPtr& op, BvType& type) override {
        return ZExtExpr::Create(op, type);
    }
    ExprPtr SExt(const ExprPtr& op, BvType& type) override {
        return SExtExpr::Create(op, type);
    }
    ExprPtr Trunc(const ExprPtr& op, BvType& type) override {
        return ExtractExpr::Create(op, 0, type.getWidth());
    }
    ExprPtr Extract(const ExprPtr& op, unsigned offset, unsigned width) override {
        return ExtractExpr::Create(op, offset, width);
    }

    ExprPtr Add(const ExprPtr& left, const ExprPtr& right) override
    {
        return ConstantFolder::Add(left, right);
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

                assert(lit != nullptr && "Operands for ANDs should be booleans!");
                if (lit->getValue() == false) {
                    return this->False();
                } else {
                    // We are not adding unnecessary true literals
                }
            } else if (op->getKind() == Expr::And) {
                // For AndExpr operands, we flatten the expression
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

        ExprRef<> x1;
        ExprRef<> e1, e2, e3;

        if (newOps.size() == 2) {
            ExprPtr lhs = newOps[0];
            ExprPtr rhs = newOps[1];

            // And(Or(E1, E2), Or(E1, E3)) --> And(E1, Or(E2, E3))
            if (match(lhs, rhs, m_Or(m_Expr(e1), m_Expr(e2)), m_Or(m_Specific(e1), m_Expr(e3)))) {
                newOps[0] = e1;
                newOps[1] = this->Or({e2, e3});
            }
        }

        // And(Eq(E1, E2), NotEq(E1, E2)) --> False
        // if (unord_match(newOps, m_Eq(m_Expr(e1), m_Expr(e2)), m_NotEq(m_Specific(e1), m_Specific(e2)))) {
        //    return this->False();
        // }

        // Move the Eq(X, C1) expressions to the front.
        auto propStart = std::partition(newOps.begin(), newOps.end(), [](const ExprRef<>& e) {
            return match(e, m_Eq(m_VarRef(), m_Literal()));
        });

        PropagationTable propTable;
        for (auto it = newOps.begin(); it != propStart; ++it) {
            ExprRef<VarRefExpr> x;
            ExprRef<LiteralExpr> lit;

            if (match(*it, m_Eq(m_VarRef(x), m_Literal(lit)))) {
                propTable.put(&x->getVariable(), *it);
            }
        }

        for (auto it = propStart; it != newOps.end(); ++it) {
            *it = PropagateExpression(*it, propTable);
        }

        return AndExpr::Create(newOps);
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
       
        return OrExpr::Create(newOps);
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

        ExprRef<BoolLiteralExpr> b1 = nullptr;
        ExprPtr c1 = nullptr;
        ExprPtr e1 = nullptr, e2 = nullptr;

        // Eq(True, X) --> X
        // Eq(False, X) --> Not(X)
        if (unord_match(left, right, m_BoolLit(b1), m_Expr(e1))) {
            if (b1->isTrue()) {
                return e1;
            }

            return this->Not(e1);
        }

        // Eq(Select(C1, E1, E2), E1) --> C1
        // Eq(Select(C1, E1, E2), E2) --> Not(C1)
        if (unord_match(left, right, m_Select(m_Expr(c1), m_Expr(e1), m_Expr(e2)), m_Specific(e1))) {
            return c1;
        }

        if (unord_match(left, right, m_Select(m_Expr(c1), m_Expr(e1), m_Expr(e2)), m_Specific(e2))) {
            return this->Not(c1);
        }

        return ConstantFolder::Eq(left, right);
    }

    ExprPtr NotEq(const ExprPtr& left, const ExprPtr& right) override
    {
        if (left == right) {
            return this->False();
        }

        ExprRef<BoolLiteralExpr> b1 = nullptr;
        ExprPtr e1 = nullptr, e2 = nullptr, e3 = nullptr;

        // NotEq(True, X) --> Not(X)
        // NotEq(False, X) --> X
        if (unord_match(left, right, m_BoolLit(b1), m_Expr(e1))) {
            if (b1->isTrue()) {
                return this->Not(e1);
            }

            return e1;
        }

        ExprRef<> x1 = nullptr, x2 = nullptr;

        // NotEq(Select(NotEq(X1, X2), E1, E2), E1) --> Eq(X)
        // NotEq(Select(NotEq(X1, X2), E1, E2), E2) --> NotEq(X)
        if (unord_match(left, right,
            m_Select(
                m_NotEq(m_Expr(x1), m_Expr(x2)),
                m_Expr(e1),
                m_Expr(e2)
            ),
            m_Expr(e3))
        ) {
            if (e3 == e1) {
                return ConstantFolder::Eq(x1, x2);
            } else if (e3 == e2) {
                return ConstantFolder::NotEq(x1, x2);
            }
        }


        return ConstantFolder::NotEq(left, right);
    }

    #define COMPARE_ARITHMETIC_SIMPLIFY(OPCODE)                             \
        ExprRef<> x;                                                        \
        llvm::APInt c1, c2;                                                 \
                                                                            \
        /* CMP(Add(X, C1), C2) --> CMP(X, C2 - C1) */                       \
        if (unord_match(left, right, m_Add(m_Bv(&c1), m_Expr(x)), m_Bv(&c2))) {   \
            return ConstantFolder::OPCODE(x, this->BvLit(c2 - c1));         \
        }                                                                   \


    ExprPtr SLt(const ExprPtr& left, const ExprPtr& right) override
    {
        return ConstantFolder::SLt(left, right);
    }

    ExprPtr SLtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        return ConstantFolder::SLtEq(left, right);
    }

    ExprPtr SGt(const ExprPtr& left, const ExprPtr& right) override
    {
        return ConstantFolder::SGt(left, right);
    }

    ExprPtr SGtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        return ConstantFolder::SGtEq(left, right);
    }

    ExprPtr ULt(const ExprPtr& left, const ExprPtr& right) override
    {
        COMPARE_ARITHMETIC_SIMPLIFY(ULt)

        return ConstantFolder::ULt(left, right);
    }

    ExprPtr ULtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        COMPARE_ARITHMETIC_SIMPLIFY(ULtEq)

        return ConstantFolder::ULtEq(left, right);
    }

    ExprPtr UGt(const ExprPtr& left, const ExprPtr& right) override
    {
        COMPARE_ARITHMETIC_SIMPLIFY(UGt)

        return ConstantFolder::UGt(left, right);
    }

    ExprPtr UGtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        COMPARE_ARITHMETIC_SIMPLIFY(UGtEq)

        return ConstantFolder::UGtEq(left, right);
    }

    ExprPtr FIsNan(const ExprPtr& op) override {
        if (op->getKind() == Expr::Literal) {
            auto fltLit = llvm::dyn_cast<FloatLiteralExpr>(op.get());
            return this->BoolLit(fltLit->getValue().isNaN());
        }

        return FIsNanExpr::Create(op);
    }

    ExprPtr FIsInf(const ExprPtr& op) override {
        if (op->getKind() == Expr::Literal) {
            auto fltLit = llvm::dyn_cast<FloatLiteralExpr>(op.get());
            return this->BoolLit(fltLit->getValue().isInfinity());
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
            auto fltLeft  = llvm::cast<FloatLiteralExpr>(left.get());
            auto fltRight = llvm::cast<FloatLiteralExpr>(right.get());

            llvm::APFloat result(fltLeft->getValue());
            result.divide(fltRight->getValue(), rm);

            return FloatLiteralExpr::Get(
                *llvm::cast<FloatType>(&left->getType()), result
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
        // Select(True, E1, E2) --> E1
        // Select(False, E1, E2) --> E2
        if (auto condLit = llvm::dyn_cast<BoolLiteralExpr>(condition.get())) {
            return condLit->isTrue() ? then : elze;
        }

        ExprPtr c1 = nullptr, c2 = nullptr;
        ExprPtr e1 = nullptr, e2 = nullptr;

        // Select(C, E, E) --> E
        if (then == elze) {
            return then;
        }

        // Select(not C, E1, E2) --> Select(C, E2, E1)
        if (match(condition, then, elze, m_Not(m_Expr(c1)), m_Expr(e1), m_Expr(e2))) {
            return ConstantFolder::Select(condition, elze, then);
        }

        // Select(C1, Select(C2, E1, E2), E2) --> Select(C1 and C2, E1, E2)
        if (match(condition, then, elze, m_Expr(c1), m_Select(m_Expr(c2), m_Expr(e1), m_Expr(e2)), m_Specific(e2))) {
            return ConstantFolder::Select(ConstantFolder::And({c1, c2}), e1, e2);
        }

        // Select(C1, E1, Select(C2, E1, E2)) --> Select(C1 or C2, E1, E2)
        if (match(condition, then, elze, m_Expr(c1), m_Expr(e1), m_Select(m_Expr(c2), m_Specific(e1), m_Expr(e2)))) {
            return ConstantFolder::Select(ConstantFolder::Or({c1, c2}), e1, e2);
        }

        return ConstantFolder::Select(condition, then, elze);
    }
};

} // end anonymous namespace

std::unique_ptr<ExprBuilder> gazer::CreateFoldingExprBuilder(GazerContext& context) {
    return std::unique_ptr<ExprBuilder>(new FoldingExprBuilder(context));
}