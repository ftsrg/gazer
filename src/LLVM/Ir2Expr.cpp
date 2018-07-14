#include "gazer/LLVM/Ir2Expr.h"

#include "gazer/Core/Type.h"
#include "gazer/Core/Utils/ExprBuilder.h"
#include "gazer/Core/Utils/CfaSimplify.h"

#include <llvm/IR/InstIterator.h>

using namespace gazer;

using llvm::Instruction;
using llvm::BasicBlock;
using llvm::Function;
using llvm::Value;
using llvm::Argument;
using llvm::GlobalVariable;
using llvm::ConstantInt;
using llvm::isa;
using llvm::dyn_cast;

gazer::Type& TypeFromLLVMType(const llvm::Value* value)
{
    if (value->getType()->isIntegerTy()) {
        auto width = value->getType()->getIntegerBitWidth();
        if (width == 1) {
            return *BoolType::get();
        }

        return *IntType::get(width);
    }

    assert(false && "Unsupported LLVM type.");
}

static bool isLogicInstruction(unsigned opcode) {
    return opcode == Instruction::And || opcode == Instruction::Or || opcode == Instruction::Xor;
}

static bool isNonConstValue(const llvm::Value* value) {
    return isa<Instruction>(value) || isa<Argument>(value) || isa<GlobalVariable>(value);
}

InstToExpr::InstToExpr(Function& function, SymbolTable& symbols, ExprBuilder* builder)
    : mFunction(function), mSymbols(symbols), mExprBuilder(builder)
{
    // Add arguments as local variables
    for (auto& arg : mFunction.args()) {
        Variable& variable = mSymbols.create(
            arg.getName(),
            TypeFromLLVMType(&arg)
        );
        mVariables[&arg] = &variable;
    }

    // Add local values as variables
    for (auto& instr : llvm::instructions(function)) {
        if (instr.getName() != "") {
            Variable& variable = mSymbols.create(
                instr.getName(),
                TypeFromLLVMType(&instr)
            );
            mVariables[&instr] = &variable;
        }
    }
}

ExprPtr InstToExpr::transform(llvm::Instruction& inst, size_t succIdx)
{
    if (inst.isTerminator()) {
        if (inst.getOpcode() == Instruction::Br) {
            return handleBr(*dyn_cast<llvm::BranchInst>(&inst), succIdx);
        }
    } else if (inst.getOpcode() == Instruction::PHI) {
        return handlePHINode(*dyn_cast<llvm::PHINode>(&inst), succIdx);
    }
    
    return transform(inst);
}

ExprPtr InstToExpr::transform(llvm::Instruction& inst)
{
    if (inst.isBinaryOp()) {
        return visitBinaryOperator(*dyn_cast<llvm::BinaryOperator>(&inst));
    } else if (inst.getOpcode() == Instruction::ICmp) {
        return visitICmpInst(*dyn_cast<llvm::ICmpInst>(&inst));    
    } else if (inst.isCast()) {
        return visitCastInst(*dyn_cast<llvm::CastInst>(&inst));
    } else if (inst.getOpcode() == Instruction::Call) {
        return visitCallInst(*dyn_cast<llvm::CallInst>(&inst));
    } else if (inst.getOpcode() == Instruction::Select) {
        return visitSelectInst(*dyn_cast<llvm::SelectInst>(&inst));
    }

    assert(false && "Unsupported instruction kind");
}

//----- Basic instruction types -----//

ExprPtr InstToExpr::visitBinaryOperator(llvm::BinaryOperator &binop)
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

            if (binop.getOpcode() == Instruction::And) {
                expr = mExprBuilder->And(boolLHS, boolRHS);
            } else if (binop.getOpcode() == Instruction::Or) {
                expr = mExprBuilder->Or(boolLHS, boolRHS);
            } else if (binop.getOpcode() == Instruction::Xor) {
                expr = mExprBuilder->Xor(boolLHS, boolRHS);
            } else {
                llvm_unreachable("Unknown logic instruction opcode");
            }
        } else {
            assert(binop.getType()->isIntegerTy()
                && "Integer operations on non-integer types");
            auto iTy = llvm::dyn_cast<llvm::IntegerType>(binop.getType());

            auto intLHS = asInt(lhs, iTy->getBitWidth());
            auto intRHS = asInt(rhs, iTy->getBitWidth());

            if (binop.getOpcode() == Instruction::And) {
                expr = mExprBuilder->BAnd(intLHS, intRHS);
            } else if (binop.getOpcode() == Instruction::Or) {
                expr = mExprBuilder->BOr(intLHS, intRHS);
            } else if (binop.getOpcode() == Instruction::Xor) {
                expr = mExprBuilder->BXor(intLHS, intRHS);
            } else {
                llvm_unreachable("Unknown logic instruction opcode");
            }
        }

        return EqExpr::Create(variable->getRefExpr(), expr);
    } else {
        const IntType* type = llvm::dyn_cast<IntType>(&variable->getType());
        assert(type && "Arithmetic results must be integer types");

        auto intLHS = asInt(lhs, type->getWidth());
        auto intRHS = asInt(rhs, type->getWidth());

        #define HANDLE_INSTCASE(OPCODE, EXPRNAME)                 \
            case OPCODE:                                          \
                expr = mExprBuilder->EXPRNAME(intLHS, intRHS);    \
                break;                                            \

        ExprPtr expr;
        switch (binop.getOpcode()) {
            HANDLE_INSTCASE(Instruction::Add, Add)
            HANDLE_INSTCASE(Instruction::Sub, Sub)
            HANDLE_INSTCASE(Instruction::Mul, Mul)
            HANDLE_INSTCASE(Instruction::Shl, Shl)
            HANDLE_INSTCASE(Instruction::LShr, LShr)
            HANDLE_INSTCASE(Instruction::AShr, AShr)
            default:
                assert(false && "Unsupported arithmetic instruction opcode");
        }

        #undef HANDLE_INSTCASE

        return EqExpr::Create(variable->getRefExpr(), expr);
    }

    llvm_unreachable("Invalid binary operation kind");
}

ExprPtr InstToExpr::visitSelectInst(llvm::SelectInst& select)
{
    Variable* selectVar = getVariable(&select);
    const Type& type = selectVar->getType();

    auto cond = asBool(operand(select.getCondition()));
    auto then = castResult(operand(select.getTrueValue()), type);
    auto elze = castResult(operand(select.getFalseValue()), type);

    return EqExpr::Create(selectVar->getRefExpr(), mExprBuilder->Select(cond, then, elze));

}

ExprPtr InstToExpr::visitICmpInst(llvm::ICmpInst& icmp)
{
    using llvm::CmpInst;
    
    auto icmpVar = getVariable(&icmp);
    auto lhs = operand(icmp.getOperand(0));
    auto rhs = operand(icmp.getOperand(1));

    auto pred = icmp.getPredicate();

    #define HANDLE_PREDICATE(PREDNAME, EXPRNAME)                \
        case PREDNAME:                                          \
            expr = mExprBuilder->EXPRNAME(lhs, rhs);            \
            break;                                              \

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
            assert(false && "Unhandled ICMP predicate.");
    }

    #undef HANDLE_PREDICATE

    return EqExpr::Create(icmpVar->getRefExpr(), expr);
}

ExprPtr InstToExpr::visitCastInst(llvm::CastInst& cast)
{
    auto variable = getVariable(&cast);
    auto intTy = llvm::dyn_cast<gazer::IntType>(&variable->getType());

    auto castOp = operand(cast.getOperand(0));
    
    unsigned width = 0;
    if (castOp->getType().isBoolType()) {
        width = 1;        
    } else if (castOp->getType().isIntType()) {
        width = dyn_cast<IntType>(&castOp->getType())->getWidth();
    } else {
        assert(false && "Unsupported cast type.");
    }

    auto intOp = asInt(castOp, width);

    if (cast.getOpcode() == Instruction::ZExt) {
        castOp = mExprBuilder->ZExt(intOp, *intTy);
    } else if (cast.getOpcode() == Instruction::SExt) {
        castOp = mExprBuilder->SExt(intOp, *intTy);
    } else if (cast.getOpcode() == Instruction::Trunc) {
        castOp = mExprBuilder->Trunc(intOp, *intTy);
    } else {
        // Other cast types are ignored at the moment.
    }

    return EqExpr::Create(variable->getRefExpr(), castOp);
}

ExprPtr InstToExpr::visitCallInst(llvm::CallInst& call)
{
    const Function* callee = call.getCalledFunction();
    assert(callee != nullptr && "Indirect calls are not supported.");

    if (callee->isDeclaration()) {
        // If this function has no definition,
        // we just replace the call with a Havoc statement
        if (call.getName() != "") {
            auto variable = getVariable(&call);
            
            return EqExpr::Create(variable->getRefExpr(), UndefExpr::Get(variable->getType()));
        }
    } else {
        assert(false && "Procedure calls are not supported (with the exception of assert).");
    }

    // XXX: This a temporary hack for no-op instructions
    return BoolLiteralExpr::getTrue();
}

//----- Branches and PHI nodes -----//
ExprPtr InstToExpr::handlePHINode(llvm::PHINode& phi, size_t succIdx)
{
    assert(phi.getNumIncomingValues() > succIdx
        && "Invalid successor index for PHI node");
    
    auto variable = getVariable(&phi);
    auto expr = operand(phi.getIncomingValue(succIdx));

    return EqExpr::Create(variable->getRefExpr(), expr);
}

ExprPtr InstToExpr::handleBr(llvm::BranchInst& br, size_t succIdx)
{
    assert((succIdx == 0 || succIdx == 1)
        && "Invalid successor index for Br");
    
    if (br.isUnconditional()) {
        return BoolLiteralExpr::getTrue();
    }

    auto cond = asBool(operand(br.getCondition()));

    if (succIdx == 0) {
        // This is the 'true' path
        return cond;
    } else {
        return NotExpr::Create(cond);
    }
}

//----- Utils and casting -----//

ExprPtr InstToExpr::operand(const Value* value)
{
    // Check for boolean literals
    if (const ConstantInt* ci = dyn_cast<ConstantInt>(value)) {
        if (ci->getType()->isIntegerTy(1)) {
            return ci->isZero() ? mExprBuilder->False() : mExprBuilder->True();
        }

        return mExprBuilder->IntLit(
            ci->getValue().getLimitedValue(),
            ci->getType()->getIntegerBitWidth()
        );
    } else if (isNonConstValue(value)) {
        return getVariable(value)->getRefExpr();
    } else if (isa<llvm::UndefValue>(value)) {
        return mExprBuilder->Undef(TypeFromLLVMType(value));
    } else {
        assert(false && "Unhandled value type");
    }
}

Variable* InstToExpr::getVariable(const Value* value)
{
    auto result = mVariables.find(value);
    assert(result != mVariables.end() && "Variables should present in the variable map.");

    return result->second;
}

ExprPtr InstToExpr::asBool(ExprPtr operand)
{
    if (operand->getType().isBoolType()) {
        return operand;
    } else if (operand->getType().isIntType()) {
        const IntType* intType = dyn_cast<IntType>(&operand->getType());
        unsigned bits = intType->getWidth();

        return mExprBuilder->Select(
            mExprBuilder->Eq(operand, mExprBuilder->IntLit(0, bits)),
            mExprBuilder->False(),
            mExprBuilder->True()
        );
    } else {
        assert(false && "Unsupported gazer type.");
    }
}

ExprPtr InstToExpr::asInt(ExprPtr operand, unsigned bits)
{
    if (operand->getType().isBoolType()) {
        return mExprBuilder->Select(
            operand,
            mExprBuilder->IntLit(1, bits),
            mExprBuilder->IntLit(0, bits)
        );
    } else if (operand->getType().isIntType()) {
        return operand;
    } else {
        assert(false && "Unsupported gazer type.");
    }
}

ExprPtr InstToExpr::castResult(ExprPtr expr, const Type& type)
{
    if (type.isBoolType()) {
        return asBool(expr);
    } else if (type.isIntType()) {
        return asInt(expr, dyn_cast<IntType>(&type)->getWidth());
    } else {
        throw TypeCastError("Invalid cast result type.");
    }
}
