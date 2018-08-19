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

gazer::Type& TypeFromLLVMType(const llvm::Type* type)
{
    if (type->isIntegerTy()) {
        auto width = type->getIntegerBitWidth();
        if (width == 1) {
            return *BoolType::get();
        }

        return *IntType::get(width);
    } else if (type->isHalfTy()) {
        return *FloatType::get(FloatType::Half);
    } else if (type->isFloatTy()) {
        return *FloatType::get(FloatType::Single);
    } else if (type->isDoubleTy()) {
        return *FloatType::get(FloatType::Double);
    } else if (type->isFP128Ty()) {
        return *FloatType::get(FloatType::Quad);
    } else if (type->isPointerTy()) {
        auto ptrTy  = llvm::cast<llvm::PointerType>(type);
        auto& elemTy = TypeFromLLVMType(ptrTy->getElementType());

        return *PointerType::get(&elemTy);
    }

    assert(false && "Unsupported LLVM type.");
}

gazer::Type& TypeFromLLVMType(const llvm::Value* value)
{
    return TypeFromLLVMType(value->getType());
}

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

InstToExpr::InstToExpr(
    Function& function,
    SymbolTable& symbols,
    ExprBuilder* builder,
    ValueToVariableMapT& variables,
    llvm::DenseMap<Variable*, llvm::Value*>* variableToValueMap
)
    : mFunction(function), mSymbols(symbols), 
    mExprBuilder(builder), mVariables(variables)
   // mStack(stack), mHeap(heap)
{
    // Add arguments as local variables
    for (auto& arg : mFunction.args()) {
        Variable& variable = mSymbols.create(
            arg.getName(),
            TypeFromLLVMType(&arg)
        );
        mVariables[&arg] = &variable;
        if (variableToValueMap != nullptr) {
            (*variableToValueMap)[&variable] = &arg;
        }
    }

    // Add local values as variables
    for (auto& instr : llvm::instructions(function)) {
        if (instr.getName() != "") {
            Variable& variable = mSymbols.create(
                instr.getName(),
                TypeFromLLVMType(&instr)
            );
            mVariables[&instr] = &variable;
            if (variableToValueMap != nullptr) {
                (*variableToValueMap)[&variable] = &instr;
            }
        }
    }
}

ExprPtr InstToExpr::transform(llvm::Instruction& inst, size_t succIdx, BasicBlock* pred)
{
    if (inst.isTerminator()) {
        if (inst.getOpcode() == Instruction::Br) {
            return handleBr(*dyn_cast<llvm::BranchInst>(&inst), succIdx);
        } else if (inst.getOpcode() == Instruction::Switch) {
            return handleSwitch(*dyn_cast<llvm::SwitchInst>(&inst), succIdx);
        }
    } else if (inst.getOpcode() == Instruction::PHI) {
        assert(pred && "Cannot handle PHIs without know predecessor");
        return handlePHINode(*dyn_cast<llvm::PHINode>(&inst), pred);
    }
    
    return transform(inst);
}

ExprPtr InstToExpr::transform(llvm::Instruction& inst)
{
    #define HANDLE_INST(OPCODE, NAME)                                   \
        else if (inst.getOpcode() == OPCODE) {                          \
            return visit##NAME(*llvm::cast<llvm::NAME>(&inst));       \
        }                                                               \

    if (inst.isBinaryOp()) {
        return visitBinaryOperator(*dyn_cast<llvm::BinaryOperator>(&inst));
    } else if (inst.isCast()) {
        return visitCastInst(*dyn_cast<llvm::CastInst>(&inst));
    }
    HANDLE_INST(Instruction::ICmp, ICmpInst)
    HANDLE_INST(Instruction::Call, CallInst)
    HANDLE_INST(Instruction::FCmp, FCmpInst)
    HANDLE_INST(Instruction::Select, SelectInst)
    HANDLE_INST(Instruction::GetElementPtr, GetElementPtrInst)

    #undef HANDLE_INST

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

        return mExprBuilder->Eq(variable->getRefExpr(), expr);
    } else if (isFloatInstruction(opcode)) {
        ExprPtr expr;
        switch (binop.getOpcode()) {
            case Instruction::FAdd:
                expr = mExprBuilder->FAdd(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
                break;
            case Instruction::FSub:
                expr = mExprBuilder->FSub(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
                break;
            case Instruction::FMul:
                expr = mExprBuilder->FMul(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
                break;
            case Instruction::FDiv:
                expr = mExprBuilder->FDiv(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
                break;
            default:
                assert(false && "Invalid floating-point operation");
        }

        return mExprBuilder->FEq(variable->getRefExpr(), expr);
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

        return mExprBuilder->Eq(variable->getRefExpr(), expr);
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

    return mExprBuilder->Eq(selectVar->getRefExpr(), mExprBuilder->Select(cond, then, elze));
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

    return mExprBuilder->Eq(icmpVar->getRefExpr(), expr);
}

ExprPtr InstToExpr::visitFCmpInst(llvm::FCmpInst& fcmp)
{
    using llvm::CmpInst;
    
    auto fcmpVar = getVariable(&fcmp);
    auto left = operand(fcmp.getOperand(0));
    auto right = operand(fcmp.getOperand(1));

    auto pred = fcmp.getPredicate();

    ExprPtr cmpExpr = nullptr;
    switch (pred) {
        case CmpInst::FCMP_OEQ:
        case CmpInst::FCMP_UEQ:
            cmpExpr = mExprBuilder->FEq(left, right);
            break;  
        case CmpInst::FCMP_OGT:
        case CmpInst::FCMP_UGT:
            cmpExpr = mExprBuilder->FGt(left, right);
            break;  
        case CmpInst::FCMP_OGE:
        case CmpInst::FCMP_UGE:
            cmpExpr = mExprBuilder->FGtEq(left, right);
            break;  
        case CmpInst::FCMP_OLT:
        case CmpInst::FCMP_ULT:
            cmpExpr = mExprBuilder->FLt(left, right);
            break;  
        case CmpInst::FCMP_OLE:
        case CmpInst::FCMP_ULE:
            cmpExpr = mExprBuilder->FLtEq(left, right);
            break;  
        case CmpInst::FCMP_ONE:
        case CmpInst::FCMP_UNE:
            cmpExpr = mExprBuilder->Not(mExprBuilder->FEq(left, right));
            break;
        default:
            break;
    }

    ExprPtr expr = nullptr;
    if (CmpInst::isOrdered(pred)) {
        // An ordered instruction can only be true if it has no NaN operands.
        expr = mExprBuilder->And({
            mExprBuilder->Not(mExprBuilder->FIsNan(left)),
            mExprBuilder->Not(mExprBuilder->FIsNan(right)),
            cmpExpr
        });
    } else if (CmpInst::isUnordered(pred)) {
        // An unordered instruction may be true if either operand is NaN
        expr = mExprBuilder->Or({
            mExprBuilder->FIsNan(left),
            mExprBuilder->FIsNan(right),
            cmpExpr
        });
    } else {
        if (pred == CmpInst::FCMP_FALSE) {
            expr = mExprBuilder->False();
        } else if (pred == CmpInst::FCMP_TRUE) {
            expr = mExprBuilder->True();
        } else if (pred == CmpInst::FCMP_ORD) {
            expr = mExprBuilder->And(
                mExprBuilder->Not(mExprBuilder->FIsNan(left)),
                mExprBuilder->Not(mExprBuilder->FIsNan(right))
            );
        } else if (pred == CmpInst::FCMP_UNO) {
            expr = mExprBuilder->Or(
                mExprBuilder->FIsNan(left),
                mExprBuilder->FIsNan(right)
            );
        } else {
            llvm_unreachable("Invalid FCmp predicate");
        }
    }

    return mExprBuilder->Eq(fcmpVar->getRefExpr(), expr);
}

ExprPtr InstToExpr::integerCast(llvm::CastInst& cast, ExprPtr operand, unsigned width)
{
    auto variable = getVariable(&cast);
    auto intTy = llvm::dyn_cast<gazer::IntType>(&variable->getType());
    
    ExprPtr intOp = asInt(operand, width);
    ExprPtr castOp = nullptr;
    if (cast.getOpcode() == Instruction::ZExt) {
        castOp = mExprBuilder->ZExt(intOp, *intTy);
    } else if (cast.getOpcode() == Instruction::SExt) {
        castOp = mExprBuilder->SExt(intOp, *intTy);
    } else if (cast.getOpcode() == Instruction::Trunc) {
        castOp = mExprBuilder->Trunc(intOp, *intTy);
    } else {
        llvm_unreachable("Unhandled integer cast operation");
    }

    return mExprBuilder->Eq(variable->getRefExpr(), castOp);
}

ExprPtr InstToExpr::visitCastInst(llvm::CastInst& cast)
{
    auto castOp = operand(cast.getOperand(0));

    if (castOp->getType().isBoolType()) {
        return integerCast(cast, castOp, 1);
    } else if (castOp->getType().isIntType()) {
        return integerCast(
            cast, castOp, dyn_cast<IntType>(&castOp->getType())->getWidth()
        );
    }

    if (cast.getOpcode() == Instruction::BitCast) {
        auto castTy = cast.getType();
        // Bitcast is no-op, just changes the type.
        // For pointer operands, this means a simple pointer cast.
        if (castOp->getType().isPointerType() && castTy->isPointerTy()) {
            return mExprBuilder->PtrCast(
                castOp,
                *llvm::dyn_cast<PointerType>(&TypeFromLLVMType(castTy))
            );
        }
    }

    assert(false && "Unsupported cast operation");
}

ExprPtr InstToExpr::visitCallInst(llvm::CallInst& call)
{
    const Function* callee = call.getCalledFunction();
    assert(callee != nullptr && "Indirect calls are not supported.");

    if (callee->isDeclaration()) {
        if (callee->getName() == "gazer.malloc") {
            auto siz = call.getArgOperand(0);
            auto start = call.getArgOperand(1);
        } else if (call.getName() != "") {
            // This is not a known function,
            // we just replace the call with an undef value
            auto variable = getVariable(&call);
            
            return mExprBuilder->Eq(variable->getRefExpr(), UndefExpr::Get(variable->getType()));
        }
    } else {
        assert(false && "Procedure calls are not supported (with the exception of assert).");
    }

    // XXX: This a temporary hack for no-op instructions
    return BoolLiteralExpr::getTrue();
}

//----- Branches and PHI nodes -----//
ExprPtr InstToExpr::handlePHINode(llvm::PHINode& phi, BasicBlock* pred)
{    
    auto variable = getVariable(&phi);
    auto expr = operand(phi.getIncomingValueForBlock(pred));

    return mExprBuilder->Eq(variable->getRefExpr(), expr);
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
        return mExprBuilder->Not(cond);
    }
}


ExprPtr InstToExpr::handleSwitch(llvm::SwitchInst& swi, size_t succIdx)
{
    ExprPtr condition = operand(swi.getCondition());
    
    llvm::SwitchInst::CaseIt caseIt = swi.case_begin();

    if (succIdx == 0) {
        // The SwitchInst is taking the default branch
        ExprPtr expr = BoolLiteralExpr::getTrue();
        while (caseIt != swi.case_end()) {
            if (caseIt != swi.case_default()) {
                ExprPtr value = operand(caseIt->getCaseValue());

                // A folding ExprBuilder will optimize this
                expr = mExprBuilder->And(
                    expr,
                    mExprBuilder->NotEq(condition, value)
                );
            }

            ++caseIt;
        }

        return expr;
    }

    // A normal case branch is taken
    while (caseIt != swi.case_end()) {
        if (caseIt->getSuccessorIndex() == succIdx) {
            break;
        }

        ++caseIt;
    }

    assert(caseIt != swi.case_end()
        && "The successor should be present in the SwitchInst");

    ExprPtr value = operand(caseIt->getCaseValue());

    return mExprBuilder->Eq(condition, value);
}

//----- Memory operations -----//
ExprPtr InstToExpr::visitStoreInst(llvm::StoreInst& store)
{
    // TODO: For now, we assume only heap operations
    auto value = operand(store.getOperand(0));
}

ExprPtr InstToExpr::visitLoadInst(llvm::LoadInst& load)
{
    auto result = getVariable(&load);
    auto index  = operand(load.getOperand(0));

    if (load.getType()->isIntegerTy()) {
        auto intTy = llvm::cast<llvm::IntegerType>(load.getType());
        if (intTy->getBitWidth() > 8) {
            // A load to an Int32 will be represented as:
            //  x[0..7] = M[addr], x[8..15] = M[addr + 1], etc.
            
            // TODO: Is this correct for all cases?
            ExprVector exprs;
            /*
            for (unsigned i = 0, j = 0; i < intTy->getBitWidth(); i += 8, ++j) {
                exprs.push_back(mExprBuilder->Eq(
                    mExprBuilder->Extract(result->getRefExpr(), i, 8),
                    ArrayReadExpr::Create(
                        mHeap.getRefExpr(),
                        mExprBuilder->Add(index, mExprBuilder->IntLit(j, 32))
                    )
                ));
            } */
            
            return mExprBuilder->And(exprs);
        }
    }

    assert(false && "Only integer arrays are supported");
}

ExprPtr InstToExpr::visitGetElementPtrInst(llvm::GetElementPtrInst& gep)
{
    ExprPtr expr = operand(gep.getOperand(0));
    for (size_t i = 1; i < gep.getNumOperands(); ++i) {
        expr = mExprBuilder->Add(expr, operand(gep.getOperand(i)));
    }

    return expr;
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
    } else if (const llvm::ConstantFP* cfp = dyn_cast<llvm::ConstantFP>(value)) {
        return FloatLiteralExpr::get(
            *llvm::dyn_cast<FloatType>(&TypeFromLLVMType(cfp->getType())),
            cfp->getValueAPF()
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
