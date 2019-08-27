#include "gazer/LLVM/InstToExpr.h"

#include <llvm/Support/Debug.h>

#define DEBUG_TYPE "InstToExpr"

using namespace gazer;
using namespace llvm;

ExprPtr InstToExpr::transform(const llvm::Instruction& inst)
{
    LLVM_DEBUG(llvm::dbgs() << "  Transforming instruction " << inst << "\n");
#define HANDLE_INST(OPCODE, NAME)                                       \
        else if (inst.getOpcode() == (OPCODE)) {                        \
            return visit##NAME(*llvm::cast<llvm::NAME>(&inst));         \
        }                                                               \

    if (inst.isBinaryOp()) {
        return visitBinaryOperator(*dyn_cast<llvm::BinaryOperator>(&inst));
    } else if (inst.isCast()) {
        return visitCastInst(*dyn_cast<llvm::CastInst>(&inst));
    } else if (auto gep = llvm::dyn_cast<llvm::GEPOperator>(&inst)) {
        return visitGEPOperator(*gep);
    }
    HANDLE_INST(Instruction::ICmp, ICmpInst)
    HANDLE_INST(Instruction::Call, CallInst)
    HANDLE_INST(Instruction::FCmp, FCmpInst)
    HANDLE_INST(Instruction::Select, SelectInst)
    HANDLE_INST(Instruction::Load, LoadInst)

#undef HANDLE_INST

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
    if (isLogicInstruction(opcode)) {
        ExprPtr expr;
        if (binop.getType()->isIntegerTy(1)) {
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
        } else {
            assert(binop.getType()->isIntegerTy()
                   && "Integer operations on non-integer types");
            auto iTy = llvm::dyn_cast<llvm::IntegerType>(binop.getType());

            auto intLHS = asInt(lhs, iTy->getBitWidth());
            auto intRHS = asInt(rhs, iTy->getBitWidth());

            switch (binop.getOpcode()) {
                case Instruction::And:
                    return mExprBuilder.BvAnd(intLHS, intRHS);
                case Instruction::Or:
                    return mExprBuilder.BvOr(intLHS, intRHS);
                case Instruction::Xor:
                    return mExprBuilder.BvXor(intLHS, intRHS);
                default:
                    llvm_unreachable("Unknown logic instruction opcode");
            }
        }

        return mExprBuilder.Eq(variable->getRefExpr(), expr);
    } else if (isFloatInstruction(opcode)) {
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
    } else {
        const BvType* type = llvm::dyn_cast<BvType>(&variable->getType());
        assert(type && "Arithmetic results must be integer types");

        auto intLHS = asInt(lhs, type->getWidth());
        auto intRHS = asInt(rhs, type->getWidth());

#define HANDLE_INSTCASE(OPCODE, EXPRNAME)                           \
            case OPCODE:                                            \
                return mExprBuilder.EXPRNAME(intLHS, intRHS);       \

        ExprPtr expr;
        switch (binop.getOpcode()) {
            HANDLE_INSTCASE(Instruction::Add, Add)
            HANDLE_INSTCASE(Instruction::Sub, Sub)
            HANDLE_INSTCASE(Instruction::Mul, Mul)
            HANDLE_INSTCASE(Instruction::SDiv, BvSDiv)
            HANDLE_INSTCASE(Instruction::UDiv, BvUDiv)
            HANDLE_INSTCASE(Instruction::SRem, BvSRem)
            HANDLE_INSTCASE(Instruction::URem, BvURem)
            HANDLE_INSTCASE(Instruction::Shl, Shl)
            HANDLE_INSTCASE(Instruction::LShr, LShr)
            HANDLE_INSTCASE(Instruction::AShr, AShr)
            default:
                LLVM_DEBUG(llvm::dbgs() << "Unsupported instruction: " << binop << "\n");
                llvm_unreachable("Unsupported arithmetic instruction opcode");
        }

#undef HANDLE_INSTCASE
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

ExprPtr InstToExpr::visitICmpInst(const llvm::ICmpInst& icmp)
{
    using llvm::CmpInst;

    auto lhs = operand(icmp.getOperand(0));
    auto rhs = operand(icmp.getOperand(1));

    auto pred = icmp.getPredicate();

#define HANDLE_PREDICATE(PREDNAME, EXPRNAME)                    \
        case PREDNAME:                                          \
            return mExprBuilder.EXPRNAME(lhs, rhs);             \

    ExprPtr expr;
    switch (pred) {
        HANDLE_PREDICATE(CmpInst::ICMP_EQ, Eq)
        HANDLE_PREDICATE(CmpInst::ICMP_NE, NotEq)
        HANDLE_PREDICATE(CmpInst::ICMP_UGT, UGt)
        HANDLE_PREDICATE(CmpInst::ICMP_UGE, UGtEq)
        HANDLE_PREDICATE(CmpInst::ICMP_ULT, ULt)
        HANDLE_PREDICATE(CmpInst::ICMP_ULE, ULtEq)
        HANDLE_PREDICATE(CmpInst::ICMP_SGT, SGt)
        HANDLE_PREDICATE(CmpInst::ICMP_SGE, SGtEq)
        HANDLE_PREDICATE(CmpInst::ICMP_SLT, SLt)
        HANDLE_PREDICATE(CmpInst::ICMP_SLE, SLtEq)
        default:
            llvm_unreachable("Unhandled ICMP predicate.");
    }

#undef HANDLE_PREDICATE
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
        expr = mExprBuilder.And({
             mExprBuilder.Not(mExprBuilder.FIsNan(left)),
             mExprBuilder.Not(mExprBuilder.FIsNan(right)),
             cmpExpr
         });
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
    //if (cast.getOperand(0)->getType()->isPointerTy()) {
    //    return mMemoryModel.handlePointerCast(cast, castOp);
    //}

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
    } else if (cast.getOpcode() == Instruction::UIToFP) {
        auto& bvTy = this->translateTypeTo<BvType>(cast.getType());
        return mExprBuilder.FpToUnsigned(castOp, bvTy, llvm::APFloat::rmNearestTiesToEven);
    } else if (castOp->getType().isBoolType()) {
        return integerCast(cast, castOp, 1);
    } else if (castOp->getType().isBvType()) {
        if (cast.getType()->isIntegerTy(1)
            && cast.getOpcode() == Instruction::Trunc
            && getVariable(&cast)->getType().isBoolType()    
        ) {
            // If the instruction truncates an integer to an i1 boolean, cast to boolean instead.
            return asBool(castOp);
        }

        return integerCast(
            cast, castOp, dyn_cast<BvType>(&castOp->getType())->getWidth()
        );
    }

    llvm_unreachable("Unsupported cast operation");
}

ExprPtr InstToExpr::integerCast(const llvm::CastInst& cast, const ExprPtr& operand, unsigned width)
{
    auto variable = getVariable(&cast);

    auto intTy = llvm::cast<gazer::BvType>(&variable->getType());

    ExprPtr intOp = asInt(operand, width);
    ExprPtr castOp = nullptr;
    if (cast.getOpcode() == Instruction::ZExt) {
        castOp = mExprBuilder.ZExt(intOp, *intTy);
    } else if (cast.getOpcode() == Instruction::SExt) {
        castOp = mExprBuilder.SExt(intOp, *intTy);
    } else if (cast.getOpcode() == Instruction::Trunc) {
        castOp = mExprBuilder.Trunc(intOp, *intTy);
    } else {
        llvm_unreachable("Unhandled integer cast operation");
    }

    return castOp;
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

    return UndefExpr::Get(callTy);
}

ExprPtr InstToExpr::visitLoadInst(const llvm::LoadInst& load)
{
    return nullptr;
}

ExprPtr InstToExpr::visitGEPOperator(const llvm::GEPOperator& gep)
{
    return nullptr;
}

ExprPtr InstToExpr::operand(const Value* value)
{
    if (const ConstantInt* ci = dyn_cast<ConstantInt>(value)) {
        // Check for boolean literals
        if (ci->getType()->isIntegerTy(1)) {
            return ci->isZero() ? mExprBuilder.False() : mExprBuilder.True();
        }

        return mExprBuilder.BvLit(
            ci->getValue().getLimitedValue(),
            ci->getType()->getIntegerBitWidth()
        );
    } else if (const llvm::ConstantFP* cfp = dyn_cast<llvm::ConstantFP>(value)) {
        return mExprBuilder.FloatLit(cfp->getValueAPF());
    } /*else if (const llvm::ConstantPointerNull* ptr = dyn_cast<llvm::ConstantPointerNull>(value)) {
        return mMemoryModel.getNullPointer();
    } */ else if (isNonConstValue(value)) {
        auto result = this->lookupInlinedVariable(value);
        if (result != nullptr) {
            return result;
        }

        return getVariable(value)->getRefExpr();
    } else if (isa<llvm::UndefValue>(value)) {
        return mExprBuilder.Undef(this->translateType(value->getType()));
    } else {
        LLVM_DEBUG(llvm::dbgs() << "  Unhandled value for operand: " << *value << "\n");
        llvm_unreachable("Unhandled value type");
    }
}

ExprPtr InstToExpr::asBool(const ExprPtr& operand)
{
    if (operand->getType().isBoolType()) {
        return operand;
    }
    
    if (operand->getType().isBvType()) {
        const BvType* bvTy = dyn_cast<BvType>(&operand->getType());
        unsigned bits = bvTy->getWidth();

        return mExprBuilder.Select(
            mExprBuilder.Eq(operand, mExprBuilder.BvLit(0, bits)),
            mExprBuilder.False(),
            mExprBuilder.True()
        );
    }

    llvm_unreachable("Attempt to cast to bool from unsupported type.");
}

ExprPtr InstToExpr::asInt(const ExprPtr& operand, unsigned int bits)
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

    llvm_unreachable("Attempt to cast to int from unsupported type.");
}

ExprPtr InstToExpr::castResult(const ExprPtr& expr, const Type& type)
{
    if (type.isBoolType()) {
        return asBool(expr);
    } else if (type.isBvType()) {
        return asInt(expr, dyn_cast<BvType>(&type)->getWidth());
    }

    llvm_unreachable("Invalid cast result type");
}

gazer::Type& InstToExpr::translateType(const llvm::Type* type)
{
    return mMemoryModel.translateType(type);
}
