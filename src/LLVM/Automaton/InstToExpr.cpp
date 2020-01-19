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
#include "gazer/LLVM/Automaton/InstToExpr.h"
#include "gazer/LLVM/Memory/MemoryModel.h"
#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include <llvm/Support/Debug.h>

#define DEBUG_TYPE "InstToExpr"

using namespace gazer;
using namespace llvm;

ExprPtr InstToExpr::transform(const llvm::Instruction& inst)
{
    LLVM_DEBUG(llvm::dbgs() << "  Transforming instruction " << inst << "\n");
    if (auto binOp = llvm::dyn_cast<llvm::BinaryOperator>(&inst)) {
        return visitBinaryOperator(*binOp);
    }
    
    if (auto cast = llvm::dyn_cast<llvm::CastInst>(&inst)) {
        return visitCastInst(*cast);
    }

#define HANDLE_INST(OPCODE, NAME)                                       \
        if (inst.getOpcode() == (OPCODE)) {                             \
            return visit##NAME(*llvm::cast<llvm::NAME>(&inst));         \
        }                                                               \

    HANDLE_INST(Instruction::ICmp,      ICmpInst)
    HANDLE_INST(Instruction::Call,      CallInst)
    HANDLE_INST(Instruction::FCmp,      FCmpInst)
    HANDLE_INST(Instruction::Select,    SelectInst)

#undef HANDLE_INST

    if (auto gep = llvm::dyn_cast<llvm::GetElementPtrInst>(&inst)) {
        std::vector<ExprPtr> ops;
        ops.reserve(gep->getNumOperands());

        for (unsigned i = 0; i < gep->getNumOperands(); ++i) {
            ops.push_back(this->operand(gep->getOperand(i)));
        }

        return mMemoryModel.handleGetElementPtr(*gep, ops);
    }

    llvm::errs() << inst << "\n";
    llvm_unreachable("Unsupported instruction kind");
}

// Transformation functions
//-----------------------------------------------------------------------------

static bool isLogicInstruction(unsigned opcode) {
    return opcode == Instruction::And || opcode == Instruction::Or || opcode == Instruction::Xor;
}

static bool isFloatInstruction(unsigned opcode) {
    return opcode == Instruction::FAdd || opcode == Instruction::FSub
           || opcode == Instruction::FMul || opcode == Instruction::FDiv;
}

static bool isNonConstValue(const llvm::Value* value) {
    return isa<Instruction>(value) || isa<Argument>(value) || isa<GlobalVariable>(value);
}

ExprPtr InstToExpr::visitBinaryOperator(const llvm::BinaryOperator& binop)
{
    auto variable = getVariable(&binop);
    auto lhs = operand(binop.getOperand(0));
    auto rhs = operand(binop.getOperand(1));
    
    auto opcode = binop.getOpcode();
    if (isLogicInstruction(opcode) && binop.getType()->isIntegerTy(1)) {
        auto boolLHS = asBool(lhs);
        auto boolRHS = asBool(rhs);

        switch (binop.getOpcode()) {
            case Instruction::And:
                return mExprBuilder.And(boolLHS, boolRHS);
            case Instruction::Or:
                return mExprBuilder.Or(boolLHS, boolRHS);
            case Instruction::Xor:
                return mExprBuilder.Xor(boolLHS, boolRHS);
            default:
                llvm_unreachable("Unknown logic instruction opcode");
        }
    }
    
    if (isFloatInstruction(opcode)) {
        ExprPtr expr;
        switch (binop.getOpcode()) {
            case Instruction::FAdd:
                return mExprBuilder.FAdd(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
            case Instruction::FSub:
                return mExprBuilder.FSub(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
            case Instruction::FMul:
                return mExprBuilder.FMul(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
            case Instruction::FDiv:
                return mExprBuilder.FDiv(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
            default:
                llvm_unreachable("Invalid floating-point operation");
        }

        return expr;
    }

    assert(variable->getType().isIntType() || variable->getType().isBvType());
    
    if (variable->getType().isBvType()) {
        const BvType* type = llvm::cast<BvType>(&variable->getType());

        auto intLHS = asBv(lhs, type->getWidth());
        auto intRHS = asBv(rhs, type->getWidth());

        #define HANDLE_INSTCASE(OPCODE, EXPRNAME)                   \
            case OPCODE:                                            \
                return mExprBuilder.EXPRNAME(intLHS, intRHS);       \

        ExprPtr expr;
        switch (binop.getOpcode()) {
            HANDLE_INSTCASE(Instruction::Add,   Add)
            HANDLE_INSTCASE(Instruction::Sub,   Sub)
            HANDLE_INSTCASE(Instruction::Mul,   Mul)
            HANDLE_INSTCASE(Instruction::SDiv,  BvSDiv)
            HANDLE_INSTCASE(Instruction::UDiv,  BvUDiv)
            HANDLE_INSTCASE(Instruction::SRem,  BvSRem)
            HANDLE_INSTCASE(Instruction::URem,  BvURem)
            HANDLE_INSTCASE(Instruction::Shl,   Shl)
            HANDLE_INSTCASE(Instruction::LShr,  LShr)
            HANDLE_INSTCASE(Instruction::AShr,  AShr)
            HANDLE_INSTCASE(Instruction::And,   BvAnd)
            HANDLE_INSTCASE(Instruction::Or,    BvOr)
            HANDLE_INSTCASE(Instruction::Xor,   BvXor)
            default:
                LLVM_DEBUG(llvm::dbgs() << "Unsupported instruction: " << binop << "\n");
                llvm_unreachable("Unsupported arithmetic instruction opcode");
        }

        #undef HANDLE_INSTCASE
    }

    if (variable->getType().isIntType()) {
        auto intLHS = asInt(lhs);
        auto intRHS = asInt(rhs);

        switch (binop.getOpcode()) {
            case Instruction::Add:
                // TODO: Add modulo to represent overflow.
                return mExprBuilder.Add(intLHS, intRHS);
            case Instruction::Sub:
                return mExprBuilder.Sub(intLHS, intRHS);
            case Instruction::Mul:
                return mExprBuilder.Mul(intLHS, intRHS);
            case Instruction::SDiv:
            case Instruction::UDiv:
                return mExprBuilder.Div(intLHS, intRHS);
            case Instruction::SRem:
            case Instruction::URem:
                return mExprBuilder.Rem(intLHS, intRHS);
            case Instruction::Shl:
            case Instruction::LShr:
            case Instruction::AShr:
            case Instruction::And:
            case Instruction::Or:
            case Instruction::Xor:
                // TODO: Some magic could be applied here to transform operations on
                // certain bit-patterns, e.g. all-ones, single-one, all-zero, single-zero, etc.
                return mExprBuilder.Undef(variable->getType());
            default:
                llvm_unreachable("Unsupported binary operator!");
        }

    }

    llvm_unreachable("Invalid binary operation kind");
}

ExprPtr InstToExpr::visitSelectInst(const llvm::SelectInst& select)
{
    Variable* selectVar = getVariable(&select);
    const Type& type = selectVar->getType();

    auto cond = asBool(operand(select.getCondition()));
    auto then = castResult(operand(select.getTrueValue()), type);
    auto elze = castResult(operand(select.getFalseValue()), type);

    return mExprBuilder.Select(cond, then, elze);
}
    
ExprPtr InstToExpr::unsignedLessThan(const ExprPtr& left, const ExprPtr& right)
{
    // We need to apply some extra care here as unsigned comparisons
    // interpret the operands as unsigned values, changing some semantics.
    // As an example, -5 < x would normally be true for x = 2. However,
    // `ult i8 -5, %x` interprets -5 (0b11111011) as unsigned, thus
    // it will be compared as 251, yielding false.

    // Given an instruction `ult(X, Y)`, this formula does the following:
    //  a) If X and Y have the same sign, then its value is X < Y.
    //  b) If X >= 0 and Y < 0, then the sign bit of X must be 0, the sign bit
    //     of Y must be 1, therefore Y will always be greater than X, thus
    //     return value is True.
    //  c) The same logic applies for the inverse case, yielding False.
    auto zero = mExprBuilder.IntLit(0);

    return mExprBuilder.Select(
        mExprBuilder.GtEq(left, zero),
        mExprBuilder.Select(
            mExprBuilder.GtEq(right, zero),
            mExprBuilder.Lt(left, right),
            mExprBuilder.True()
        ),
        mExprBuilder.Select(
            mExprBuilder.Lt(right, zero),
            mExprBuilder.Lt(left, right),
            mExprBuilder.False()
        )
    );
}

ExprPtr InstToExpr::visitICmpInst(const llvm::ICmpInst& icmp)
{
    using llvm::CmpInst;

    auto pred = icmp.getPredicate();

    auto left = operand(icmp.getOperand(0));
    auto right = operand(icmp.getOperand(1));

    if (pred == CmpInst::ICMP_EQ) {
        return mExprBuilder.Eq(left, right);
    }

    if (pred == CmpInst::ICMP_NE) {
        return mExprBuilder.NotEq(left, right);
    }

    #define HANDLE_PREDICATE(PREDNAME, EXPRNAME)                \
        case PREDNAME:                                          \
            return mExprBuilder.EXPRNAME(left, right);          \

    if (left->getType().isBvType()) {
        switch (pred) {
            HANDLE_PREDICATE(CmpInst::ICMP_UGT, BvUGt)
            HANDLE_PREDICATE(CmpInst::ICMP_UGE, BvUGtEq)
            HANDLE_PREDICATE(CmpInst::ICMP_ULT, BvULt)
            HANDLE_PREDICATE(CmpInst::ICMP_ULE, BvULtEq)
            HANDLE_PREDICATE(CmpInst::ICMP_SGT, BvSGt)
            HANDLE_PREDICATE(CmpInst::ICMP_SGE, BvSGtEq)
            HANDLE_PREDICATE(CmpInst::ICMP_SLT, BvSLt)
            HANDLE_PREDICATE(CmpInst::ICMP_SLE, BvSLtEq)
            default:
                llvm_unreachable("Unknown ICMP predicate.");
        }
    }

    #undef HANDLE_PREDICATE

    if (left->getType().isArithmetic()) {
        switch (pred) {
            case CmpInst::ICMP_UGT:
                return unsignedLessThan(right, left);
            case CmpInst::ICMP_SGT:
                return mExprBuilder.Gt(left, right);
            case CmpInst::ICMP_UGE:
                return mExprBuilder.Or(
                    mExprBuilder.Eq(left, right),
                    unsignedLessThan(right, left)
                );
            case CmpInst::ICMP_SGE:
                return mExprBuilder.GtEq(left, right);
            case CmpInst::ICMP_ULT:
                return unsignedLessThan(left, right);
            case CmpInst::ICMP_SLT:
                return mExprBuilder.Lt(left, right);
            case CmpInst::ICMP_ULE:
                return mExprBuilder.Or(
                    mExprBuilder.Eq(left, right),
                    unsignedLessThan(left, right)
                );
            case CmpInst::ICMP_SLE:
                return mExprBuilder.LtEq(left, right);
            default:
                llvm_unreachable("Unknown ICMP predicate.");
        }
    }

    llvm_unreachable("Invalid type for comparison instruction!");
}

ExprPtr InstToExpr::visitFCmpInst(const llvm::FCmpInst& fcmp)
{
    using llvm::CmpInst;

    auto left = operand(fcmp.getOperand(0));
    auto right = operand(fcmp.getOperand(1));

    auto pred = fcmp.getPredicate();

    ExprPtr cmpExpr = nullptr;
    switch (pred) {
        case CmpInst::FCMP_OEQ:
        case CmpInst::FCMP_UEQ:
            cmpExpr = mExprBuilder.FEq(left, right);
            break;
        case CmpInst::FCMP_OGT:
        case CmpInst::FCMP_UGT:
            cmpExpr = mExprBuilder.FGt(left, right);
            break;
        case CmpInst::FCMP_OGE:
        case CmpInst::FCMP_UGE:
            cmpExpr = mExprBuilder.FGtEq(left, right);
            break;
        case CmpInst::FCMP_OLT:
        case CmpInst::FCMP_ULT:
            cmpExpr = mExprBuilder.FLt(left, right);
            break;
        case CmpInst::FCMP_OLE:
        case CmpInst::FCMP_ULE:
            cmpExpr = mExprBuilder.FLtEq(left, right);
            break;
        case CmpInst::FCMP_ONE:
        case CmpInst::FCMP_UNE:
            cmpExpr = mExprBuilder.Not(mExprBuilder.FEq(left, right));
            break;
        default:
            break;
    }

    ExprPtr expr = nullptr;
    if (pred == CmpInst::FCMP_FALSE) {
        expr = mExprBuilder.False();
    } else if (pred == CmpInst::FCMP_TRUE) {
        expr = mExprBuilder.True();
    } else if (pred == CmpInst::FCMP_ORD) {
        expr = mExprBuilder.And(
            mExprBuilder.Not(mExprBuilder.FIsNan(left)),
            mExprBuilder.Not(mExprBuilder.FIsNan(right))
        );
    } else if (pred == CmpInst::FCMP_UNO) {
        expr = mExprBuilder.Or(
            mExprBuilder.FIsNan(left),
            mExprBuilder.FIsNan(right)
        );
    } else if (CmpInst::isOrdered(pred)) {
        // An ordered instruction can only be true if it has no NaN operands.
        // As our comparison operators are defined to be false if either
        // argument is NaN, we we can just return the compare expression.
        expr = cmpExpr;
    } else if (CmpInst::isUnordered(pred)) {
        // An unordered instruction may be true if either operand is NaN
        expr = mExprBuilder.Or({
            mExprBuilder.FIsNan(left),
            mExprBuilder.FIsNan(right),
            cmpExpr
        });
    } else {
        llvm_unreachable("Invalid FCmp predicate");
    }

    return expr;
}

ExprPtr InstToExpr::visitCastInst(const llvm::CastInst& cast)
{
    auto castOp = operand(cast.getOperand(0));

    if (cast.getType()->isFloatingPointTy()) {
        auto& fltTy = this->translateTypeTo<FloatType>(cast.getType());

        switch (cast.getOpcode()) {
            case Instruction::FPExt:
            case Instruction::FPTrunc:
                return mExprBuilder.FCast(castOp, fltTy, llvm::APFloat::rmNearestTiesToEven);
            case Instruction::SIToFP:
                return mExprBuilder.SignedToFp(castOp, fltTy, llvm::APFloat::rmNearestTiesToEven);
            case Instruction::UIToFP:
                return mExprBuilder.UnsignedToFp(castOp, fltTy, llvm::APFloat::rmNearestTiesToEven);
            default:
                break;
        }
    }

    if (cast.getOpcode() == Instruction::FPToSI) {
        auto& bvTy = this->translateTypeTo<BvType>(cast.getType());
        return mExprBuilder.FpToSigned(castOp, bvTy, llvm::APFloat::rmNearestTiesToEven);
    }
    
    if (cast.getOpcode() == Instruction::FPToUI) {
        auto& bvTy = this->translateTypeTo<BvType>(cast.getType());
        return mExprBuilder.FpToUnsigned(castOp, bvTy, llvm::APFloat::rmNearestTiesToEven);
    }
    
    if (cast.getType()->isPointerTy()) {
        auto origPtr = operand(cast.getOperand(0));
        return mMemoryModel.handlePointerCast(cast, origPtr);
    }

    if (castOp->getType().isBoolType()) {
        return boolToIntCast(cast, castOp);
    }
    
    // If the instruction truncates an integer to an i1 boolean, cast to boolean instead.
    if (cast.getType()->isIntegerTy(1)
        && cast.getOpcode() == Instruction::Trunc
        && getVariable(&cast)->getType().isBoolType()    
    ) {
        return asBool(castOp);
    }

    if (castOp->getType().isBvType()) {
        return integerCast(
            cast, castOp, dyn_cast<BvType>(&castOp->getType())->getWidth()
        );
    }

    if (castOp->getType().isIntType()) {
        // ZExt and SExt are no-op in this case.
        if (cast.getOpcode() == Instruction::ZExt || cast.getOpcode() == Instruction::SExt) {
            return castOp;
        }

        if (cast.getOpcode() == Instruction::Trunc) {
            // We can get the lower 'w' bits of 'n' if we do 'n mod 2^w'.
            // However, due to LLVM's two's complement representation, this
            // could turn into a signed number.
            // For example:
            //  trunc i6 51 to i4: 11|0011 --> 3
            //  trunc i6 60 to i4: 11|1100 --> -4
            // To overcome this, we check the sign bit of the resulting value
            // and if it set, we substract '2^w' from the result.
            auto maxVal = mExprBuilder.IntLit(
                llvm::APInt::getMaxValue(cast.getType()->getIntegerBitWidth()).getZExtValue()
            );
            auto maxValDiv2 = mExprBuilder.IntLit(
                llvm::APInt::getMaxValue(cast.getType()->getIntegerBitWidth() - 1).getZExtValue()
            );
            auto modVal = mExprBuilder.Mod(castOp, maxVal);

            return mExprBuilder.Select(
                mExprBuilder.Eq(
                    mExprBuilder.Mod(
                        mExprBuilder.Div(castOp, maxValDiv2),
                        mExprBuilder.IntLit(2)
                    ),
                    mExprBuilder.IntLit(0)
                ),
                modVal,
                mExprBuilder.Sub(modVal, maxVal)
            );
        }

        return mExprBuilder.Undef(castOp->getType());
    }

    if (cast.getOpcode() == Instruction::BitCast) {
        // TODO...
    }

    llvm_unreachable("Unsupported cast operation");
}

ExprPtr InstToExpr::integerCast(const llvm::CastInst& cast, const ExprPtr& operand, unsigned width)
{
    auto variable = getVariable(&cast);

    if (auto bvTy = llvm::dyn_cast<gazer::BvType>(&variable->getType())) {
        ExprPtr intOp = asBv(operand, width);

        switch (cast.getOpcode()) {
            case Instruction::ZExt:
                return mExprBuilder.ZExt(intOp, *bvTy);
            case Instruction::SExt:
                return mExprBuilder.SExt(intOp, *bvTy);
            case Instruction::Trunc:
                return mExprBuilder.Trunc(intOp, *bvTy);
            default:
                llvm_unreachable("Unhandled integer cast operation");
        }
    }

    llvm_unreachable("Invalid bit-vector type!");
}

ExprPtr InstToExpr::boolToIntCast(const llvm::CastInst& cast, const ExprPtr& operand)
{
    auto variable = getVariable(&cast);

    auto one  = llvm::APInt{1, 1};
    auto zero = llvm::APInt{1, 0};

    if (auto bvTy = dyn_cast<gazer::BvType>(&variable->getType())) {
        switch (cast.getOpcode())
        {
            case Instruction::ZExt:
                return mExprBuilder.Select(
                    operand,
                    mExprBuilder.BvLit(one.zext(bvTy->getWidth())),
                    mExprBuilder.BvLit(zero.zext(bvTy->getWidth()))
                );
            case Instruction::SExt:
                return mExprBuilder.Select(
                    operand,
                    mExprBuilder.BvLit(one.sext(bvTy->getWidth())),
                    mExprBuilder.BvLit(zero.sext(bvTy->getWidth()))
                );
            default:
                llvm_unreachable("Invalid integer cast operation");
        }
    }

    if (auto intTy = dyn_cast<gazer::IntType>(&variable->getType())) {
        switch (cast.getOpcode())
        {
            case Instruction::ZExt:
                return mExprBuilder.Select(
                    operand,
                    mExprBuilder.IntLit(1),
                    mExprBuilder.IntLit(0)
                );
            case Instruction::SExt: {
                // In two's complement 111..11 corresponds to -1, 111..10 to -2
                return mExprBuilder.Select(
                    operand,
                    mExprBuilder.IntLit(-1),
                    mExprBuilder.IntLit(-2)
                );
            }
            default:
                llvm_unreachable("Invalid integer cast operation");
        }
    }
    
    llvm_unreachable("Invalid integer cast type!");
}

static GazerIntrinsic::Overflow getOverflowKind(llvm::StringRef name)
{
    #define HANDLE_PREFIX(PREFIX, KIND)                                           \
        if (name.startswith(PREFIX)) { return GazerIntrinsic::Overflow::KIND; } \

    HANDLE_PREFIX(GazerIntrinsic::SAddNoOverflowPrefix, SAdd)
    HANDLE_PREFIX(GazerIntrinsic::SSubNoOverflowPrefix, SSub)
    HANDLE_PREFIX(GazerIntrinsic::SMulNoOverflowPrefix, SMul)
    HANDLE_PREFIX(GazerIntrinsic::SDivNoOverflowPrefix, SDiv)

    #undef HANDLE_PREFIX

    llvm_unreachable("Unknown overflow check!");
}

static ExprPtr handleSAddOverflow(const ExprPtr& left, const ExprPtr& right, ExprBuilder& builder)
{
    auto& bvType = llvm::cast<BvType>(left->getType());
    unsigned width = bvType.getWidth();

    auto& newType = BvType::Get(left->getContext(), width + 1);

    ExprPtr el = builder.SExt(left, newType);
    ExprPtr er = builder.SExt(right, newType);

    ExprPtr result = builder.Add(el, er);

    return builder.And(
        builder.BvSLt(result, builder.BvLit(llvm::APInt::getSignedMaxValue(width).sext(width + 1))),
        builder.BvSGt(result, builder.BvLit(llvm::APInt::getSignedMinValue(width).sext(width + 1)))
    );
}

static ExprPtr handleSSubOverflow(const ExprPtr& left, const ExprPtr& right, ExprBuilder& builder)
{
    auto& bvType = llvm::cast<BvType>(left->getType());
    auto neg = builder.Mul(
        builder.BvLit(llvm::APInt::getAllOnesValue(bvType.getWidth())),
        right
    );

    return handleSAddOverflow(left, neg, builder);
}

static ExprPtr handleSDivOverflow(const ExprPtr& left, const ExprPtr& right, ExprBuilder& builder)
{
    auto& bvType = llvm::cast<BvType>(left->getType());
    unsigned width = bvType.getWidth();

    auto& newType = BvType::Get(left->getContext(), width + 1);

    ExprPtr el = builder.SExt(left, newType);
    ExprPtr er = builder.SExt(right, newType);

    ExprPtr result = builder.BvSDiv(el, er);

    return builder.And(
        builder.BvSLt(result, builder.BvLit(llvm::APInt::getSignedMaxValue(width).sext(width + 1))),
        builder.BvSGt(result, builder.BvLit(llvm::APInt::getSignedMinValue(width).sext(width + 1)))
    );
}

static ExprPtr handleSMulOverflow(const ExprPtr& left, const ExprPtr& right, ExprBuilder& builder)
{
    auto& bvType = llvm::cast<BvType>(left->getType());
    unsigned width = bvType.getWidth();

    auto& newType = BvType::Get(left->getContext(), 2 * width);

    ExprPtr el = builder.SExt(left, newType);
    ExprPtr er = builder.SExt(right, newType);

    ExprPtr result = builder.Mul(el, er);

    return builder.And(
        builder.BvSLt(result, builder.BvLit(llvm::APInt::getSignedMaxValue(width).sext(2 * width))),
        builder.BvSGt(result, builder.BvLit(llvm::APInt::getSignedMinValue(width).sext(2 * width)))
    );
}

ExprPtr InstToExpr::handleOverflowPredicate(const llvm::CallInst& call)
{
    Function* callee = call.getCalledFunction();
    assert(callee != nullptr);

    ExprPtr left  = this->operand(call.getArgOperand(0));
    ExprPtr right = this->operand(call.getArgOperand(1));
    
    GazerIntrinsic::Overflow kind = getOverflowKind(callee->getName());

    if (left->getType().isIntType()) {
        ExprPtr result;
        switch (kind) {
            case GazerIntrinsic::Overflow::SAdd: result = mExprBuilder.Add(left, right); break;
            case GazerIntrinsic::Overflow::SSub: result = mExprBuilder.Sub(left, right); break;
            case GazerIntrinsic::Overflow::SMul: result = mExprBuilder.Mul(left, right); break;
            case GazerIntrinsic::Overflow::SDiv: result = mExprBuilder.Div(left, right); break;
            default:
                llvm_unreachable("Unknown overflow kind!");
        }

        unsigned width = call.getArgOperand(0)->getType()->getIntegerBitWidth();

        // TODO: We should use a BigInteger implementation here instead of clamping an APInt to int64_t
        auto min = llvm::APInt::getSignedMinValue(width).getSExtValue();
        auto max = llvm::APInt::getSignedMaxValue(width).getSExtValue();

        // Operands are representable => Result must be representable as well
        auto ops = mExprBuilder.And({
            mExprBuilder.GtEq(left, mExprBuilder.IntLit(min)),
            mExprBuilder.LtEq(left, mExprBuilder.IntLit(max)),
            mExprBuilder.GtEq(right, mExprBuilder.IntLit(min)),
            mExprBuilder.LtEq(right, mExprBuilder.IntLit(max))
        });

        return mExprBuilder.Imply(ops, mExprBuilder.And(
            mExprBuilder.GtEq(result, mExprBuilder.IntLit(min)),
            mExprBuilder.LtEq(result, mExprBuilder.IntLit(max))
        ));
    }

    if (left->getType().isBvType()) {
        switch (kind) {
            case GazerIntrinsic::Overflow::SAdd: return handleSAddOverflow(left, right, mExprBuilder);
            case GazerIntrinsic::Overflow::SSub: return handleSSubOverflow(left, right, mExprBuilder);
            case GazerIntrinsic::Overflow::SMul: return handleSMulOverflow(left, right, mExprBuilder);
            case GazerIntrinsic::Overflow::SDiv: return handleSDivOverflow(left, right, mExprBuilder);
            default:
                llvm_unreachable("Unknown overflow kind!");
        }
    }
    
    llvm_unreachable("Invalid type!");
}

ExprPtr InstToExpr::visitCallInst(const llvm::CallInst& call)
{
    gazer::Type& callTy = this->translateType(call.getType());

    const Function* callee = call.getCalledFunction();
    if (callee == nullptr) {
        return UndefExpr::Get(callTy);
        // This is an indirect call, use the memory model to resolve it.
        //return mMemoryModel.handleCall(call);
    }

    if (callee->getName().startswith(GazerIntrinsic::NoOverflowPrefix)) {
        return this->handleOverflowPredicate(call);
    }

    return UndefExpr::Get(callTy);
}

ExprPtr InstToExpr::operand(ValueOrMemoryObject value)
{
    if (value.isValue()) {
        return this->operandValue(value.asValue());
    }

    if (value.isMemoryObjectDef()) {
        return this->operandMemoryObject(value.asMemoryObjectDef());
    }

    llvm_unreachable("Invalid ValueOrMemoryObject state!");
}

ExprPtr InstToExpr::operandValue(const llvm::Value* value)
{
    if (auto ci = dyn_cast<ConstantInt>(value)) {
        // Check for boolean literals
        if (ci->getType()->isIntegerTy(1)) {
            return ci->isZero() ? mExprBuilder.False() : mExprBuilder.True();
        }

        switch (mSettings.ints) {
            case IntRepresentation::BitVectors:
                return mExprBuilder.BvLit(
                    ci->getValue().getLimitedValue(),
                    ci->getType()->getIntegerBitWidth()
                );
            case IntRepresentation::Integers:
                return mExprBuilder.IntLit(ci->getSExtValue());
        }

        llvm_unreachable("Invalid int representation strategy!");
    }
    
    if (auto cfp = dyn_cast<llvm::ConstantFP>(value)) {
        return mExprBuilder.FloatLit(cfp->getValueAPF());
    }

    if (auto ca = dyn_cast<llvm::ConstantDataArray>(value)) {
        // Translate each element in the array
        std::vector<ExprRef<LiteralExpr>> elements;
        elements.reserve(ca->getNumElements());
        for (unsigned i = 0; i < ca->getNumElements(); ++i) {
            llvm::Constant* constantElem = ca->getElementAsConstant(i);
            ExprPtr constantExpr = this->operandValue(constantElem);

            assert(llvm::isa<LiteralExpr>(constantExpr)
                && "Constants should be translated to literals!");

            elements.push_back(expr_cast<LiteralExpr>(constantExpr));
        }

        return mMemoryModel.handleConstantDataArray(ca, elements);
    }

    // Non-instruction pointer values should be resolved using the memory model
    if (value->getType()->isPointerTy()
        && !llvm::isa<llvm::Instruction>(value)
        && !llvm::isa<llvm::Argument>(value)) {
        return mMemoryModel.handlePointerValue(value, mFunction);
    }
    
    if (isNonConstValue(value)) {
        auto result = this->lookupInlinedVariable(value);
        if (result != nullptr) {
            return result;
        }

        return getVariable(value)->getRefExpr();
    }
    
    if (isa<llvm::UndefValue>(value)) {
        return mExprBuilder.Undef(this->translateType(value->getType()));
    }
    
    LLVM_DEBUG(llvm::dbgs() << "  Unhandled value for operand: " << *value << "\n");
    llvm_unreachable("Unhandled value type");
}

ExprPtr InstToExpr::operandMemoryObject(const gazer::MemoryObjectDef* def)
{
    auto result = this->lookupInlinedVariable(def);
    if (result != nullptr) {
        return result;
    }

    return getVariable(def)->getRefExpr();
}

ExprPtr InstToExpr::asBool(const ExprPtr& operand)
{
    if (operand->getType().isBoolType()) {
        return operand;
    }
    
    if (operand->getType().isBvType()) {
        auto bvTy = cast<BvType>(&operand->getType());
        unsigned bits = bvTy->getWidth();

        return mExprBuilder.Select(
            mExprBuilder.Eq(operand, mExprBuilder.BvLit(0, bits)),
            mExprBuilder.False(),
            mExprBuilder.True()
        );
    }

    if (operand->getType().isIntType()) {
        return mExprBuilder.Select(
            mExprBuilder.Eq(operand, mExprBuilder.IntLit(0)),
            mExprBuilder.False(),
            mExprBuilder.True()
        );
    }

    llvm_unreachable("Attempt to cast to bool from unsupported type.");
}

ExprPtr InstToExpr::asBv(const ExprPtr& operand, unsigned int bits)
{
    if (operand->getType().isBoolType()) {
        return mExprBuilder.Select(
            operand,
            mExprBuilder.BvLit(1, bits),
            mExprBuilder.BvLit(0, bits)
        );
    }
    
    if (operand->getType().isBvType()) {
        return operand;
    }

    llvm_unreachable("Attempt to cast to bitvector from unsupported type.");
}

ExprPtr InstToExpr::asInt(const ExprPtr& operand)
{
    if (operand->getType().isBoolType()) {
        return mExprBuilder.Select(
            operand,
            mExprBuilder.IntLit(1),
            mExprBuilder.IntLit(0)
        );
    }
    
    if (operand->getType().isIntType()) {
        return operand;
    }

    llvm_unreachable("Attempt to cast to int from unsupported type.");
}

ExprPtr InstToExpr::castResult(const ExprPtr& expr, const Type& type)
{
    if (expr->getType() == type) {
        return expr;
    }

    if (type.isBoolType()) {
        return asBool(expr);
    }
    
    if (auto bvTy = llvm::dyn_cast<BvType>(&type)) {
        return asBv(expr, bvTy->getWidth());
    }

    if (type.isIntType()) {
        return asInt(expr);
    }

    llvm_unreachable("Invalid cast result type");
}

gazer::Type& InstToExpr::translateType(const llvm::Type* type)
{
    return mMemoryModel.translateType(type);
}
