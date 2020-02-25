//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
///
/// \file This file defines the FoldingExprBuilder class, an implementation of
/// the expression builder interface. It aims to fold constant expressions and
/// perform some basic formula simplification.
///
//===----------------------------------------------------------------------===//
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Expr/Matcher.h"
#include "gazer/ADT/Algorithm.h"

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/DenseSet.h>
#include <llvm/Support/raw_ostream.h>

#include <boost/container/flat_set.hpp>

#include <unordered_set>
#include <variant>

using namespace gazer;
using namespace gazer::PatternMatch;

using llvm::isa;
using llvm::cast;
using llvm::dyn_cast;

namespace
{

class FoldingExprBuilder : public ExprBuilder
{
public:
    FoldingExprBuilder(GazerContext& context)
        : ExprBuilder(context)
    {}

private:
    ExprPtr foldBinaryExpr(Expr::ExprKind kind, const ExprPtr& left, const ExprPtr& right);
    ExprPtr foldBinaryCompare(Expr::ExprKind kind, const ExprPtr& left, const ExprPtr& right);
    ExprPtr simplifyLtEq(const ExprPtr& left, const ExprPtr& right);

public:
    ExprPtr Not(const ExprPtr& op) override
    {
        if (auto boolLit = dyn_cast<BoolLiteralExpr>(op.get())) {
            return BoolLiteralExpr::Get(op->getContext(), !boolLit->getValue());
        }

        ExprPtr x, y;

        // Not(Or(Not(X), Y)) --> And(X, Not(Y))
        if (match(op, m_Or(m_Not(m_Expr(x)), m_Expr(y)))) {
            return this->And({x, this->Not(y)});
        }

        return NotExpr::Create(op);
    }

    ExprPtr ZExt(const ExprPtr& op, BvType& type) override
    {
        if (auto bvLit = dyn_cast<BvLiteralExpr>(op.get())) {
            return BvLiteralExpr::Get(type, bvLit->getValue().zext(type.getWidth()));
        }

        return ZExtExpr::Create(op, type);
    }
    
    ExprPtr SExt(const ExprPtr& op, BvType& type) override
    {
        if (auto bvLit = dyn_cast<BvLiteralExpr>(op.get())) {
            return BvLiteralExpr::Get(type, bvLit->getValue().sext(type.getWidth()));
        }

        return SExtExpr::Create(op, type);
    }

    ExprPtr Extract(const ExprPtr& op, unsigned offset, unsigned width) override
    {
        if (auto bvLit = dyn_cast<BvLiteralExpr>(op)) {
            return BvLiteralExpr::Get(
                BvType::Get(op->getContext(), width),
                bvLit->getValue().extractBits(width, offset)
            );
        }

        if (op->isUndef()) {
            // Extracting from and undef results in an undef
            return UndefExpr::Get(BvType::Get(getContext(), width));
        }

        return ExtractExpr::Create(op, offset, width);
    }

    #define FOLD_BINARY_ARITHMETIC(KIND)                                    \
    ExprPtr KIND(const ExprPtr& left, const ExprPtr& right) override {      \
        ExprPtr folded = this->foldBinaryExpr(Expr::KIND, left, right);   \
        if (folded != nullptr) { return folded; }                           \
        return KIND##Expr::Create(left, right);                             \
    }

    FOLD_BINARY_ARITHMETIC(Add)
    FOLD_BINARY_ARITHMETIC(Sub)
    FOLD_BINARY_ARITHMETIC(Mul)
    FOLD_BINARY_ARITHMETIC(BvSDiv)
    FOLD_BINARY_ARITHMETIC(BvUDiv)
    FOLD_BINARY_ARITHMETIC(BvSRem)
    FOLD_BINARY_ARITHMETIC(BvURem)
    FOLD_BINARY_ARITHMETIC(Shl)
    FOLD_BINARY_ARITHMETIC(LShr)
    FOLD_BINARY_ARITHMETIC(AShr)
    FOLD_BINARY_ARITHMETIC(BvAnd)
    FOLD_BINARY_ARITHMETIC(BvOr)
    FOLD_BINARY_ARITHMETIC(BvXor)

    #undef FOLD_BINARY_ARITHMETIC

    ExprPtr BvConcat(const ExprPtr& left, const ExprPtr& right) override
    {
        unsigned width = cast<BvType>(left->getType()).getWidth() + cast<BvType>(right->getType()).getWidth();
        
        if (auto lhsLit = llvm::dyn_cast<BvLiteralExpr>(left)) {
            if (auto rhsLit = llvm::dyn_cast<BvLiteralExpr>(right)) {
                llvm::APInt result = lhsLit->getValue().zext(width).shl(rhsLit->getType().getWidth());
                result = result | rhsLit->getValue().zext(width);

                return this->BvLit(result);
            }
        }

        if (left->isUndef() && right->isUndef()) {
            // We do not want to simplify partially undef cases.
            return this->Undef(BvType::Get(getContext(), width));
        }

        return BvConcatExpr::Create(left, right);
    }

    ExprPtr And(const ExprVector& vector) override
    {
        ExprVector newOps;

        for (const ExprPtr& op : vector) {
            if (auto lit = dyn_cast<BoolLiteralExpr>(op)) {
                // We are not adding unnecessary true literals
                if (lit->isFalse()) {
                    return this->False();
                }                
            } else if (auto andExpr = dyn_cast<AndExpr>(op)) {
                // For AndExpr operands, we flatten the expression
                newOps.insert(newOps.end(), andExpr->op_begin(), andExpr->op_end());
            } else {
                newOps.push_back(op);
            }
        }

        if (newOps.empty()) {
            // If we eliminated all operands
            return this->True();
        }
        
        if (newOps.size() == 1) {
            return *newOps.begin();
        }

        return AndExpr::Create(newOps);
    }

    ExprPtr Or(const ExprVector& vector) override
    {
        ExprVector newOps;

        for (const ExprPtr& op : vector) {
            if (auto lit = dyn_cast<BoolLiteralExpr>(op)) {
                if (lit->isTrue()) {
                    return this->True();
                } else {
                    // We are not adding unnecessary false literals
                }
            } else if (auto orExpr = dyn_cast<OrExpr>(op)) {
                // For OrExpr operands, we try to flatten the expression
                newOps.insert(newOps.end(), orExpr->op_begin(), orExpr->op_end());
            } else {
                newOps.push_back(op);
            }
        }

        if (newOps.empty()) {
            // If we eliminated all operands
            return this->False();
        }
        
        if (newOps.size() == 1) {
            return *newOps.begin();
        }

        return OrExpr::Create(newOps);
    }

    ExprPtr Imply(const ExprPtr& left, const ExprPtr& right) override
    {
        // True  => X --> X
        // False => X --> True
        if (auto bl = llvm::dyn_cast<BoolLiteralExpr>(left)) {
            if (bl->isTrue()) {
                return right;
            }

            return this->True();
        }

        // X => True  --> True
        // X => False --> not X
        if (auto br = llvm::dyn_cast<BoolLiteralExpr>(right)) {
            if (br->isTrue()) {
                return this->True();
            }

            return this->Not(left);
        }

        return ImplyExpr::Create(left, right);
    }

    ExprPtr Eq(const ExprPtr& left, const ExprPtr& right) override
    {
        if (left == right) {
            return this->True();
        }

        if (isa<LiteralExpr>(left) && isa<LiteralExpr>(right) && left != right) {
            return this->False();
        }

        if (ExprPtr folded = this->foldBinaryCompare(Expr::Eq, left, right)) {
            return folded;
        }

        ExprRef<BoolLiteralExpr> b1 = nullptr;
        ExprPtr c1 = nullptr;
        ExprRef<LiteralExpr> l1 = nullptr, l2 = nullptr;

        // Eq(Select(C1, L1, L2), L1) --> C1
        if (unord_match(left, right, m_Select(m_Expr(c1), m_Literal(l1), m_Literal(l2)), m_Specific(l1))) {
            return c1;
        }

        // Eq(Select(C1, L1, L2), L2) --> Not(C1)
        if (unord_match(left, right, m_Select(m_Expr(c1), m_Literal(l1), m_Literal(l2)), m_Specific(l2))) {
            return this->Not(c1);
        }

        return EqExpr::Create(left, right);
    }

    ExprPtr NotEq(const ExprPtr& left, const ExprPtr& right) override
    {
        return this->Not(this->Eq(left, right));
    }

    // Comparison operators
    //===------------------------------------------------------------------===//
    // Apart from folding, we shall also normalize everything here
    // into a LtEq expression.

    ExprPtr Lt(const ExprPtr& left, const ExprPtr& right) override
    {
        // Lt(X, Y) --> Not(LtEq(Y, X))
        return this->Not(this->LtEq(right, left));
    }

    ExprPtr LtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        if (ExprPtr folded = this->foldBinaryCompare(Expr::LtEq, left, right)) {
            return folded;
        }

        return this->simplifyLtEq(left, right);
    }

    ExprPtr Gt(const ExprPtr& left, const ExprPtr& right) override
    {
        if (ExprPtr folded = this->foldBinaryCompare(Expr::Gt, left, right)) {
            return folded;
        }

        // Gt(C, X) --> LtEq(X, C - 1) if C is Int
        IntLiteralExpr::ValueTy intVal;
        ExprPtr x;
        if (match(left, right, m_Int(&intVal), m_Expr(x))) {
            return LtEqExpr::Create(x, this->IntLit(intVal - 1));
        }

        // Gt(X, Y) --> Not(LtEq(X, Y))
        return this->Not(this->LtEq(left, right));
    }

    ExprPtr GtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        // GtEq(X, Y) --> LtEq(Y, X)
        return this->LtEq(right, left);
    }

    ExprPtr BvSLt(const ExprPtr& left, const ExprPtr& right) override
    {
        return BvSLtExpr::Create(left, right);
    }

    ExprPtr BvSLtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        return BvSLtEqExpr::Create(left, right);
    }

    ExprPtr BvSGt(const ExprPtr& left, const ExprPtr& right) override
    {
        return BvSGtExpr::Create(left, right);
    }

    ExprPtr BvSGtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        return BvSGtEqExpr::Create(left, right);
    }

    ExprPtr BvULt(const ExprPtr& left, const ExprPtr& right) override
    {
        return BvULtExpr::Create(left, right);
    }

    ExprPtr BvULtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        return BvULtEqExpr::Create(left, right);
    }

    ExprPtr BvUGt(const ExprPtr& left, const ExprPtr& right) override
    {
        return BvUGtExpr::Create(left, right);
    }

    ExprPtr BvUGtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        return BvUGtEqExpr::Create(left, right);
    }

    // Floating-point
    //===------------------------------------------------------------------===//

    ExprPtr FIsNan(const ExprPtr& op) override
    {
        if (auto fpLit = dyn_cast<FloatLiteralExpr>(op)) {
            return this->BoolLit(fpLit->getValue().isNaN());
        }

        return FIsInfExpr::Create(op);
    }

    ExprPtr FIsInf(const ExprPtr& op) override
    {
        if (auto fpLit = dyn_cast<FloatLiteralExpr>(op)) {
            return this->BoolLit(fpLit->getValue().isInfinity());
        }

        return FIsInfExpr::Create(op);
    }
    
    ExprPtr FCast(const ExprPtr& op, FloatType& type, llvm::APFloat::roundingMode rm) override
    {
        return FCastExpr::Create(op, type, rm);
    }

    ExprPtr SignedToFp(const ExprPtr& op, FloatType& type, llvm::APFloat::roundingMode rm) override {
        return SignedToFpExpr::Create(op, type, rm);
    }

    ExprPtr UnsignedToFp(const ExprPtr& op, FloatType& type, llvm::APFloat::roundingMode rm) override {
        return UnsignedToFpExpr::Create(op, type, rm);
    }

    ExprPtr FpToSigned(const ExprPtr& op, BvType& type, llvm::APFloat::roundingMode rm) override {
        return FpToSignedExpr::Create(op, type, rm);
    }

    ExprPtr FpToUnsigned(const ExprPtr& op, BvType& type, llvm::APFloat::roundingMode rm) override {
        return FpToUnsignedExpr::Create(op, type, rm);
    }
    
    ExprPtr FAdd(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override
    {
        return FAddExpr::Create(left, right, rm);
    }
    
    ExprPtr FSub(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override
    {
        return FSubExpr::Create(left, right, rm);
    }

    ExprPtr FMul(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override
    {
        return FMulExpr::Create(left, right, rm);
    }

    ExprPtr FDiv(const ExprPtr& left, const ExprPtr& right, llvm::APFloat::roundingMode rm) override
    {
        return FDivExpr::Create(left, right, rm);
    }
    
    ExprPtr FEq(const ExprPtr& left, const ExprPtr& right) override
    {
        return FEqExpr::Create(left, right);
    }
    ExprPtr FGt(const ExprPtr& left, const ExprPtr& right) override
    {
        return FGtExpr::Create(left, right);
    }
    ExprPtr FGtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        return FGtEqExpr::Create(left, right);
    }
    ExprPtr FLt(const ExprPtr& left, const ExprPtr& right) override
    {
        return FLtExpr::Create(left, right);
    }
    ExprPtr FLtEq(const ExprPtr& left, const ExprPtr& right) override
    {
        return FLtEqExpr::Create(left, right);
    }

    ExprPtr Select(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze) override
    {
        // Select(True, E1, E2) --> E1
        // Select(False, E1, E2) --> E2
        if (auto condLit = dyn_cast<BoolLiteralExpr>(condition.get())) {
            return condLit->isTrue() ? then : elze;
        }

        ExprPtr c1 = nullptr, c2 = nullptr;
        ExprPtr e1 = nullptr, e2 = nullptr;

        // Select(C, E, E) --> E
        if (then == elze) {
            return then;
        }

        // Select(C, E, False) --> And(C, E)
        if (elze == this->False()) {
            return this->And({ condition, then });
        }

        // Select(C, E, True) --> Or(not C, E)
        if (elze == this->True()) {
            return this->Or({ this->Not(condition), then });
        }

        // Select(C, True, E) --> Or(C, E)
        if (then == this->True()) {
            return this->Or({ condition, elze });
        }

        // Select(C, False, E) --> And(not C, E)
        if (then == this->False()) {
            return this->And({ this->Not(condition), elze });
        }

        // Select(not C, E1, E2) --> Select(C, E2, E1)
        if (match(condition, then, elze, m_Not(m_Expr(c1)), m_Expr(e1), m_Expr(e2))) {
            return SelectExpr::Create(c1, elze, then);
        }

        // Select(C1, Select(C1, E1, E'), E2) --> Select(C1, E1, E2)
        if (match(condition, then, elze, m_Expr(c1), m_Select(m_Specific(c1), m_Expr(e1), m_Expr()), m_Expr(e2))) {
            return SelectExpr::Create(c1, e1, e2);
        }

        // Select(C1, E1, Select(C1, E', E2)) --> Select(C1, E1, E2)
        if (match(condition, then, elze, m_Expr(c1), m_Expr(e1), m_Select(m_Specific(c1), m_Expr(), m_Expr(e2)))) {
            return SelectExpr::Create(c1, e1, e2);
        }

        // Select(C1, Select(C2, E1, E2), E1) --> Select(C1 and not C2, E2, E1)
        if (match(condition, then, elze, m_Expr(c1), m_Select(m_Expr(c2), m_Expr(e1), m_Expr(e2)), m_Specific(e1))) {
            return SelectExpr::Create(this->And({c1, this->Not(c2)}), e1, e2);
        }
    
        // Select(C1, Select(C2, E1, E2), E2) --> Select(C1 and C2, E1, E2)
        if (match(condition, then, elze, m_Expr(c1), m_Select(m_Expr(c2), m_Expr(e1), m_Expr(e2)), m_Specific(e2))) {
            return SelectExpr::Create(AndExpr::Create({c1, c2}), e1, e2);
        }

        // Select(C1, E1, Select(C2, E1, E2)) --> Select(C1 or C2, E1, E2)
        if (match(condition, then, elze, m_Expr(c1), m_Expr(e1), m_Select(m_Expr(c2), m_Specific(e1), m_Expr(e2)))) {
            return SelectExpr::Create(OrExpr::Create({c1, c2}), e1, e2);
        }

        return SelectExpr::Create(condition, then, elze);
    }

    ExprPtr Write(const ExprPtr& array, const ExprPtr& index, const ExprPtr& value) override
    {
        if (auto lit = llvm::dyn_cast<ArrayLiteralExpr>(array)) {
            if (lit->getMap().empty()) {
                if ((!lit->hasDefault() && value->isUndef()) || lit->getDefault() == value) {
                    return lit;
                }
            }
        }

        return ArrayWriteExpr::Create(array, index, value);
    }
    
    ExprPtr Read(const ExprPtr& array, const ExprPtr& index) override
    {
        if (auto lit = llvm::dyn_cast<ArrayLiteralExpr>(array)) {
            if (lit->getMap().empty()) {
                if (lit->hasDefault()) {
                    return lit->getDefault();
                }

                return UndefExpr::Get(lit->getType().getElementType());
            }
        }

        return ArrayReadExpr::Create(array, index);
    }
};

} // end anonymous namespace

ExprPtr FoldingExprBuilder::foldBinaryExpr(Expr::ExprKind kind, const ExprPtr& left, const ExprPtr& right)
{
    if (auto rhs = dyn_cast<IntLiteralExpr>(right)) {
        // If the right side is an integer literal
        switch (kind) {
            case Expr::Add:
            case Expr::Sub:
                if (rhs->isZero()) { return left; }
                break;
            case Expr::Mul:
                if (rhs->isZero()) { return right; }
                if (rhs->isOne()) { return left; }
                break;
            case Expr::Div:
                if (rhs->isOne()) { return left; }
                break;
            case Expr::Mod:
            case Expr::Rem:
                if (rhs->isOne()) { return this->IntLit(0); }
                break;
            default:
                break;
        }
    } else if (auto rhs = dyn_cast<BvLiteralExpr>(right)) {
        // Alternatively, it can be a bit-vector literal
        switch (kind) {
            case Expr::Add:
            case Expr::Sub:
                if (rhs->isZero()) { return left; }
                break;
            case Expr::Mul:
                if (rhs->isZero()) { return right; }
                if (rhs->isOne())  { return left; }
                break;
            case Expr::BvUDiv:
            case Expr::BvSDiv:
                if (rhs->isOne()) { return left; }                          // X div 1 == X
                break;
            case Expr::BvURem:
            case Expr::BvSRem:
                if (rhs->isOne()) { return BvLiteralExpr::Get(rhs->getType(), 0); }   // X rem 1 == 0
                break;
            case Expr::BvAnd:
                if (rhs->isZero()) { return rhs; }                          // X and 0 == 0
                if (rhs->isAllOnes()) { return left; }                      // X and 1..1 == X
                break;
            case Expr::BvOr:
                if (rhs->isZero()) { return left; }                         // X or 0 == X
                if (rhs->isAllOnes()) { return rhs; }                       // X and 1..1 == 1..1 
            case Expr::BvXor:
                if (rhs->isZero()) { return left; }                         // X xor 0 == X
                break;
            default:
                break;
        }
    } else if (llvm::isa<LiteralExpr>(left) && Expr::isCommutative(kind)) {
        // If LHS is constant while RHS is not, retry with swapped operands
        return foldBinaryExpr(kind, right, left);
    }

    // See if both of them are literals
    if (auto lhs = dyn_cast<IntLiteralExpr>(left)) {
        if (auto rhs = dyn_cast<IntLiteralExpr>(right)) {
            auto l = lhs->getValue();
            auto r = rhs->getValue();

            switch (kind) {
                case Expr::Add: return this->IntLit(l + r);
                case Expr::Sub: return this->IntLit(l - r);
                case Expr::Mul: return this->IntLit(l * r);
                case Expr::Div: return this->IntLit(l / r);
                case Expr::Mod:
                case Expr::Rem:
                // FIXME
                default:
                    break;
            }
        }
    } else if (auto lhs = dyn_cast<BvLiteralExpr>(left)) {
        if (auto rhs = dyn_cast<BvLiteralExpr>(right)) {
            auto l = lhs->getValue();
            auto r = rhs->getValue();

            switch (kind) {
                case Expr::Add: return this->BvLit(l + r);
                case Expr::Sub: return this->BvLit(l - r);
                case Expr::Mul: return this->BvLit(l * r);
                case Expr::BvUDiv: return this->BvLit(l.udiv(r));
                case Expr::BvSDiv: return this->BvLit(l.sdiv(r));
                case Expr::BvURem: return this->BvLit(l.urem(r));
                case Expr::BvSRem: return this->BvLit(l.srem(r));
                case Expr::BvAnd: return this->BvLit(l & r);
                case Expr::BvOr: return this->BvLit(l | r);
                case Expr::BvXor: return this->BvLit(l ^ r);
                // FIXME: This is problematic if shift width exceeds the size
                case Expr::Shl: return this->BvLit(l.shl(r));
                case Expr::LShr: return this->BvLit(l.lshr(r));
                case Expr::AShr: return this->BvLit(l.ashr(r));
                default:
                    llvm_unreachable("Unknown binary BV expression!");
            }
        }

        switch (kind) {
            case Expr::BvSDiv:
            case Expr::BvUDiv:
            case Expr::BvSRem:
            case Expr::BvURem:
            case Expr::LShr:
            case Expr::AShr:
            case Expr::Shl:
                if (lhs->isZero()) { return lhs; }
                break;
            default:
                break;
        }
    }

    // Give up, cannot fold.
    return nullptr;
}

ExprPtr FoldingExprBuilder::foldBinaryCompare(Expr::ExprKind kind, const ExprPtr& left, const ExprPtr& right)
{
    if (isa<BoolLiteralExpr>(right)) {
        switch (kind) {
            case Expr::Eq:
                // Eq(X, True) --> X
                // Eq(X, False) --> Not(X)
                if (auto blit = dyn_cast<BoolLiteralExpr>(right)) {
                    if (blit->isTrue()) {
                        return left;
                    }

                    return this->Not(left);
                }
                break;
            case Expr::NotEq:
                // NotEq(X, True) --> Not(X)
                // NotEq(X, False) --> X
                if (auto blit = dyn_cast<BoolLiteralExpr>(right)) {
                    if (blit->isTrue()) {
                        return this->Not(left);
                    }

                    return left;
                }
                break;
            default:
                break;
        }
    } else if (isa<BoolLiteralExpr>(left)) {
        // If LHS is a literal, swap the operands around and check through the other way
        return foldBinaryCompare(kind, right, left);
    }

    if (isa<BvLiteralExpr>(left) && isa<BvLiteralExpr>(right)) {
        auto lv = cast<BvLiteralExpr>(left)->getValue();
        auto rv = cast<BvLiteralExpr>(right)->getValue();

        switch (kind) {
            case Expr::BvSLt:   return this->BoolLit(lv.slt(rv));
            case Expr::BvSLtEq: return this->BoolLit(lv.sle(rv));
            case Expr::BvSGt:   return this->BoolLit(lv.sgt(rv));
            case Expr::BvSGtEq: return this->BoolLit(lv.sge(rv));
            case Expr::BvULt:   return this->BoolLit(lv.ult(rv));
            case Expr::BvULtEq: return this->BoolLit(lv.ule(rv));
            case Expr::BvUGt:   return this->BoolLit(lv.ugt(rv));
            case Expr::BvUGtEq: return this->BoolLit(lv.uge(rv));
            case Expr::Eq:      return this->BoolLit(lv == rv);
            case Expr::NotEq:   return this->BoolLit(lv != rv);
            default:
                llvm_unreachable("Unknown bit-vector comparison!");
        }
    } else if (isa<IntLiteralExpr>(left) && isa<IntLiteralExpr>(right)) {
        auto lv = cast<IntLiteralExpr>(left)->getValue();
        auto rv = cast<IntLiteralExpr>(right)->getValue();

        switch (kind) {
            case Expr::Eq:      return this->BoolLit(lv == rv);
            case Expr::NotEq:   return this->BoolLit(lv != rv);
            case Expr::Lt:      return this->BoolLit(lv < rv);
            case Expr::LtEq:    return this->BoolLit(lv <= rv);
            case Expr::Gt:      return this->BoolLit(lv > rv);
            case Expr::GtEq:    return this->BoolLit(lv >= rv);
            default:
                llvm_unreachable("Unknown integer comparison!");
        }
    }

    return nullptr;
}

ExprPtr FoldingExprBuilder::simplifyLtEq(const ExprPtr& left, const ExprPtr& right)
{
    ExprPtr x, other;
    IntLiteralExpr::ValueTy c1;
    IntLiteralExpr::ValueTy c2;

    if (auto rhs = dyn_cast<IntLiteralExpr>(right)) {
        c2 = rhs->getValue();
        other = left;
    } else if (auto lhs = dyn_cast<IntLiteralExpr>(left)) {
        c2 = lhs->getValue();
        other = right;
    } else {
        return LtEqExpr::Create(left, right);
    }

    #define CREATE_RESULT(VAR, LIT)                                                         \
        (other == left ? LtEqExpr::Create((VAR), (LIT)) : LtEqExpr::Create((LIT), (VAR)))

    // X + C1 <= C2 --> X <= C2 - C1
    // C2 <= X + C1 --> C2 - C1 <= X
    if (match(other, m_Add(m_Int(&c1), m_Expr(x)))) {
        return CREATE_RESULT(x, this->IntLit(c2 - c1));
    }

    // X - C1 <= C2 --> X <= C1 + C2
    // C2 <= X - C1 --> C2 + C1 <= X
    if (match(other, m_Sub(m_Expr(x), m_Int(&c1)))) {
        return CREATE_RESULT(x, this->IntLit(c2 + c1));
    }

    return LtEqExpr::Create(left, right);
}

std::unique_ptr<ExprBuilder> gazer::CreateFoldingExprBuilder(GazerContext& context) {
    return std::unique_ptr<ExprBuilder>(new FoldingExprBuilder(context));
}