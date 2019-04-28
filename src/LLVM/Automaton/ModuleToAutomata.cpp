#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/Automaton/Cfa.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/ADT/StringUtils.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Debug.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Dominators.h>
#include <llvm/ADT/StringExtras.h>

#include "FunctionToCfa.h"

#define DEBUG_TYPE "ModuleToCfa"

namespace gazer
{

class ModuleToCfa final
{
public:
    using LoopInfoMapTy = llvm::DenseMap<llvm::Function*, llvm::LoopInfo*>;

    ModuleToCfa(
        llvm::Module& module,
        llvm::CallGraph& cg,
        LoopInfoMapTy& loops,
        GazerContext& context
    )
        : mModule(module), mCallGraph(cg),
        mLoops(loops), mContext(context)
    {
        mSystem = std::make_unique<AutomataSystem>(context);
    }

    std::unique_ptr<AutomataSystem> generate();

private:
    Cfa* encodeFunction(llvm::Function& function, llvm::LoopInfo& loopInfo);

    Cfa* encodeLoop(Cfa* parent, llvm::Loop* loop, llvm::LoopInfo& loopInfo);

    gazer::Type& typeFromLLVMType(const llvm::Type* type);

    gazer::Type& typeFromLLVMType(const llvm::Value* value);

private:
    llvm::Module& mModule;
    llvm::CallGraph& mCallGraph;
    LoopInfoMapTy& mLoops;

    GazerContext& mContext;
    std::unique_ptr<AutomataSystem> mSystem;

    // Generation helpers
    std::unordered_map<llvm::Function*, Cfa*> mFunctionMap;
    std::unordered_map<llvm::Loop*, Cfa*> mLoopMap;

    ValueToVariableMap mVariables;
};

} // end namespace gazer

using namespace gazer;
using namespace llvm;

static bool isDefinedInCaller(llvm::Value* value, llvm::ArrayRef<llvm::BasicBlock*> blocks)
{
    if (isa<Argument>(value)) {
        return true;
    }

    if (auto i = dyn_cast<Instruction>(value)) {
        if (std::find(blocks.begin(), blocks.end(), i->getParent()) != blocks.end()) {
            return false;
        }

        return true;
    }

    // TODO: Is this always correct?
    return false;
}

std::unique_ptr<AutomataSystem> ModuleToCfa::generate()
{
    GenerationContext genCtx(*mSystem);
    auto exprBuilder = CreateFoldingExprBuilder(mContext);

    // First, add all global variables
    for (llvm::GlobalVariable& gv : mModule.globals()) {
        // TODO...
    }

    // Create an automaton for each function definition
    // and set the interfaces.
    for (llvm::Function& function : mModule.functions()) {
        if (function.isDeclaration()) {
            continue;
        }

        function.viewCFG();

        Cfa* cfa = mSystem->createCfa(function.getName());

        DenseSet<BasicBlock*> visitedBlocks;

        // Create a CFA for each loop nested in this function
        LoopInfo* loopInfo = mLoops[&function];

        auto loops = loopInfo->getLoopsInPreorder();
        for (Loop* loop : loops) {
            Cfa* nested = mSystem->createNestedCfa(cfa, loop->getName());
            CfaGenInfo& loopGenInfo = genCtx.LoopMap.try_emplace(loop).first->second;
            loopGenInfo.Automaton = nested;
        }

        for (auto li = loops.rbegin(), le = loops.rend(); li != le; ++li) {
            Loop* loop = *li;
            CfaGenInfo& loopGenInfo = genCtx.LoopMap[loop];

            Cfa* nested = loopGenInfo.Automaton;

            ArrayRef<BasicBlock*> loopBlocks = loop->getBlocks();

            // Insert the loop variables
            for (BasicBlock* bb : loopBlocks) {
                for (Instruction& inst : *bb) {
                    for (auto oi = inst.op_begin(), oe = inst.op_end(); oi != oe; ++oi) {
                        llvm::Value* value = *oi;
                        if (isDefinedInCaller(value, loopBlocks)) {
                            auto variable = nested->addInput(value->getName(), typeFromLLVMType(value->getType()));
                            loopGenInfo.Inputs[value] = variable;

                            LLVM_DEBUG(llvm::dbgs() << "  Added input variable " << *variable << "\n");
                        }
                    }

                    if (inst.getType()->isVoidTy()) {
                        continue;
                    }

                    bool isLocal = true;
                    for (auto user : inst.users()) {
                        if (auto i = llvm::dyn_cast<Instruction>(user)) {
                            if (std::find(loopBlocks.begin(), loopBlocks.end(), i->getParent()) == loopBlocks.end()) {
                                auto variable = nested->addOutput(inst.getName(), typeFromLLVMType(i->getType()));
                                loopGenInfo.Outputs[&inst] = variable;
                                isLocal = false;

                                LLVM_DEBUG(llvm::dbgs() << "  Added output variable " << *variable << "\n");
                                break;
                            }
                        }
                    }

                    // If the instruction is defined in an already visited block, it is a local of
                    // a subloop rather than this loop.
                    if (isLocal && inst.getName() != "" && visitedBlocks.count(bb) == 0) {
                        auto variable = nested->addLocal(inst.getName(), typeFromLLVMType(inst.getType()));
                        loopGenInfo.Locals[&inst] = variable;

                        LLVM_DEBUG(llvm::dbgs() << "  Added local variable " << *variable << "\n");
                    }
                }

                // Create locations for this block
                if (visitedBlocks.count(bb)  == 0) {
                    Location* entry = nested->createLocation();
                    Location* exit = nested->createLocation();

                    loopGenInfo.Blocks[bb] = std::make_pair(entry, exit);
                }
            }

            std::vector<BasicBlock*> blocksToEncode;
            std::copy_if(
                loopBlocks.begin(), loopBlocks.end(),
                std::back_inserter(blocksToEncode),
                [&visitedBlocks] (BasicBlock* b) { return visitedBlocks.count(b) == 0; }
            );

            visitedBlocks.insert(loop->getBlocks().begin(), loop->getBlocks().end());

            // Do the actual encoding
            LLVM_DEBUG(llvm::dbgs() << "Encoding CFA " << nested->getName() << "\n");
            BlocksToCfa blocksToCfa(
                genCtx,
                loopGenInfo,
                blocksToEncode,
                nested,
                *exprBuilder
            );
            blocksToCfa.encode(loop->getHeader());
        }

        CfaGenInfo& genInfo = genCtx.FunctionMap.try_emplace(&function).first->second;
        genInfo.Automaton = cfa;

        // Add function input and output parameters
        for (llvm::Argument& argument : function.args()) {
            Variable* variable = cfa->addInput(argument.getName(), typeFromLLVMType(argument.getType()));
            genInfo.Inputs[&argument] = variable;
        }

        // TODO: Maybe add RET_VAL to genInfo outputs in some way?
        cfa->addOutput("RET_VAL", typeFromLLVMType(function.getReturnType()));

        // For the local variables, we only need to add the values not present
        // in any of the loops.
        for (BasicBlock& bb : function) {
            if (loopInfo->getLoopFor(&bb) != nullptr) {
                continue;
            }

            for (Instruction& inst : bb) {
                if (inst.getName() != "") {
                    Variable* variable = cfa->addLocal(inst.getName(), typeFromLLVMType(inst.getType()));
                    genInfo.Locals[&inst] = variable;
                }
            }

            Location* entry = cfa->createLocation();
            Location* exit = cfa->createLocation();

            genInfo.Blocks[&bb] = std::make_pair(entry, exit);
        }

        // Now do the actual encoding.
        // First, we start with the loops.
        //for (auto li = loops.rbegin(), le = loops.rend(); li != le; ++li) {
        //   Loop* loop = *li;

        //    CfaGenInfo& loopToCfaInfo = genCtx.LoopMap[loop];

        //}
    }

    return std::move(mSystem);
}

void BlocksToCfa::encode(llvm::BasicBlock* entryBlock)
{
    assert(std::find(mBlocks.begin(), mBlocks.end(), entryBlock) != mBlocks.end()
        && "Entry block must be in the block list!");
    assert(mGenInfo.Blocks.count(entryBlock) != 0
        && "Entry block must be present in block map!");

    Location* first = mGenInfo.Blocks[entryBlock].first;

    // Create a transition between the initial location and the entry block.
    mCfa->createAssignTransition(mCfa->getEntry(), first, mExprBuilder.True());

    for (BasicBlock* bb : mBlocks) {
        Location* entry = mGenInfo.Blocks[bb].first;
        Location* exit = mGenInfo.Blocks[bb].second;

        std::vector<VariableAssignment> assignments;
        assignments.reserve(bb->size());

        for (auto it = bb->getFirstInsertionPt(); it != bb->end(); ++it) {
            llvm::Instruction& inst = *it;

            if (inst.getType()->isVoidTy()) {
                continue;
            }

            Variable* variable = getVariable(&inst);
            ExprPtr expr = this->transform(inst);

            assignments.emplace_back(variable, expr);
        }

        mCfa->createAssignTransition(entry, exit, assignments);

        // Handle the outgoing edges
        auto terminator = bb->getTerminator();

        if (auto br = llvm::dyn_cast<BranchInst>(terminator)) {
            ExprPtr condition = br->isConditional() ? operand(br->getCondition()) : mExprBuilder.True();

            for (int succIdx = 0; succIdx < br->getNumSuccessors(); ++succIdx) {
                BasicBlock* succ = br->getSuccessor(succIdx);
                ExprPtr succCondition = succIdx == 0 ? condition : mExprBuilder.Not(condition);

                if (succ == entryBlock) {
                    // If the target is the loop header (entry block), create a call to this same automaton.
                    auto loc = mCfa->createLocation();

                    mCfa->createAssignTransition(exit, loc, succCondition);
                    mCfa->createCallTransition(loc, mCfa->getExit(), mCfa, {}, {});
                } else if (std::find(mBlocks.begin(), mBlocks.end(), succ) != mBlocks.end()) {
                    // Else if the target is is inside the block region, just create a simple edge.
                    Location* to = mGenInfo.Blocks[succ].first;

                    std::vector<VariableAssignment> phiAssignments;
                    insertPhiAssignments(bb, succ, phiAssignments);

                    mCfa->createAssignTransition(exit, to, succCondition);
                } else {
                    mCfa->createAssignTransition(exit, mCfa->getExit(), succCondition);
                }
            }
        }

    }
}

ExprPtr BlocksToCfa::transform(llvm::Instruction& inst)
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
    }
    HANDLE_INST(Instruction::ICmp, ICmpInst)
    //HANDLE_INST(Instruction::Call, CallInst)
    HANDLE_INST(Instruction::FCmp, FCmpInst)
    HANDLE_INST(Instruction::Select, SelectInst)

#undef HANDLE_INST

    llvm_unreachable("Unsupported instruction kind");
}

void BlocksToCfa::insertPhiAssignments(
    BasicBlock* source,
    BasicBlock* target,
    std::vector<VariableAssignment>& phiAssignments)
{
    auto it = target->begin();
    while (llvm::isa<PHINode>(it)) {
        PHINode* phi = llvm::dyn_cast<PHINode>(it);
        Value* incoming = phi->getIncomingValueForBlock(source);

        Variable* variable = getVariable(phi);
        ExprPtr expr = operand(incoming);

        phiAssignments.emplace_back(variable, expr);

        ++it;
    }
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

ExprPtr BlocksToCfa::visitBinaryOperator(llvm::BinaryOperator &binop)
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
                return mExprBuilder.And(boolLHS, boolRHS);
            } else if (binop.getOpcode() == Instruction::Or) {
                return mExprBuilder.Or(boolLHS, boolRHS);
            } else if (binop.getOpcode() == Instruction::Xor) {
                return mExprBuilder.Xor(boolLHS, boolRHS);
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
                return mExprBuilder.BAnd(intLHS, intRHS);
            } else if (binop.getOpcode() == Instruction::Or) {
                return mExprBuilder.BOr(intLHS, intRHS);
            } else if (binop.getOpcode() == Instruction::Xor) {
                return mExprBuilder.BXor(intLHS, intRHS);
            } else {
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
                assert(false && "Invalid floating-point operation");
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
            HANDLE_INSTCASE(Instruction::SDiv, SDiv)
            HANDLE_INSTCASE(Instruction::UDiv, UDiv)
            HANDLE_INSTCASE(Instruction::SRem, SRem)
            HANDLE_INSTCASE(Instruction::URem, URem)
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

ExprPtr BlocksToCfa::visitSelectInst(llvm::SelectInst& select)
{
    Variable* selectVar = getVariable(&select);
    const Type& type = selectVar->getType();

    auto cond = asBool(operand(select.getCondition()));
    auto then = castResult(operand(select.getTrueValue()), type);
    auto elze = castResult(operand(select.getFalseValue()), type);

    return mExprBuilder.Select(cond, then, elze);
}

ExprPtr BlocksToCfa::visitICmpInst(llvm::ICmpInst& icmp)
{
    using llvm::CmpInst;

    auto icmpVar = getVariable(&icmp);
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

ExprPtr BlocksToCfa::visitFCmpInst(llvm::FCmpInst& fcmp)
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

ExprPtr BlocksToCfa::integerCast(llvm::CastInst& cast, ExprPtr operand, unsigned width)
{
    auto variable = getVariable(&cast);
    auto intTy = llvm::dyn_cast<gazer::BvType>(&variable->getType());

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

ExprPtr BlocksToCfa::visitCastInst(llvm::CastInst& cast)
{
    auto castOp = operand(cast.getOperand(0));
    //if (cast.getOperand(0)->getType()->isPointerTy()) {
    //    return mMemoryModel.handlePointerCast(cast, castOp);
    //}

    if (castOp->getType().isBoolType()) {
        return integerCast(cast, castOp, 1);
    } else if (castOp->getType().isBvType()) {
        return integerCast(
            cast, castOp, dyn_cast<BvType>(&castOp->getType())->getWidth()
        );
    }

    assert(false && "Unsupported cast operation");
}
ExprPtr BlocksToCfa::operand(const Value* value)
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
    } /* else if (const llvm::ConstantFP* cfp = dyn_cast<llvm::ConstantFP>(value)) {
        return FloatLiteralExpr::Get(
            *llvm::dyn_cast<FloatType>(&typeFromLLVMType(cfp->getType())),
            cfp->getValueAPF()
        );
    } else if (const llvm::ConstantPointerNull* ptr = dyn_cast<llvm::ConstantPointerNull>(value)) {
        return mMemoryModel.getNullPointer();
    } */ else if (isNonConstValue(value)) {
        //auto result = mEliminatedValues.find(value);
        //if (result != mEliminatedValues.end()) {
        //    return result->second;
        //}

        return getVariable(value)->getRefExpr();
    } /* else if (isa<llvm::UndefValue>(value)) {
        return mExprBuilder.Undef(typeFromLLVMType(value));
    } */else {
        LLVM_DEBUG(llvm::dbgs() << "  Unhandled value for operand: " << *value << "\n");
        assert(false && "Unhandled value type");
    }
}

Variable* BlocksToCfa::getVariable(const Value* value)
{
    LLVM_DEBUG(llvm::dbgs() << "Getting variable for value " << *value << "\n");

    auto result = mGenInfo.Inputs.find(value);
    if (result != mGenInfo.Inputs.end()) {
        return result->second;
    }

    result = mGenInfo.Outputs.find(value);
    if (result != mGenInfo.Outputs.end()) {
        return result->second;
    }

    result = mGenInfo.Locals.find(value);
    if (result != mGenInfo.Locals.end()) {
        return result->second;
    }

    llvm_unreachable("Variables should be present in one of the variable maps!");
}

ExprPtr BlocksToCfa::asBool(ExprPtr operand)
{
    if (operand->getType().isBoolType()) {
        return operand;
    } else if (operand->getType().isBvType()) {
        const BvType* bvTy = dyn_cast<BvType>(&operand->getType());
        unsigned bits = bvTy->getWidth();

        return mExprBuilder.Select(
            mExprBuilder.Eq(operand, mExprBuilder.BvLit(0, bits)),
            mExprBuilder.False(),
            mExprBuilder.True()
        );
    } else {
        assert(false && "Unsupported gazer type.");
    }
}

ExprPtr BlocksToCfa::asInt(ExprPtr operand, unsigned bits)
{
    if (operand->getType().isBoolType()) {
        return mExprBuilder.Select(
            operand,
            mExprBuilder.BvLit(1, bits),
            mExprBuilder.BvLit(0, bits)
        );
    } else if (operand->getType().isBvType()) {
        return operand;
    } else {
        assert(false && "Unsupported gazer type.");
    }
}

ExprPtr BlocksToCfa::castResult(ExprPtr expr, const Type& type)
{
    if (type.isBoolType()) {
        return asBool(expr);
    } else if (type.isBvType()) {
        return asInt(expr, dyn_cast<BvType>(&type)->getWidth());
    } else {
        assert(!"Invalid cast result type");
    }
}

gazer::Type& ModuleToCfa::typeFromLLVMType(const llvm::Type* type)
{
    if (type->isIntegerTy()) {
        auto width = type->getIntegerBitWidth();
        if (width == 1) {
            return BoolType::Get(mContext);
        }

        //if (UseMathInt && width <= 64) {
        //    return IntType::Get(mContext, width);
        //}

        return BvType::Get(mContext, width);
    } else if (type->isHalfTy()) {
        return FloatType::Get(mContext, FloatType::Half);
    } else if (type->isFloatTy()) {
        return FloatType::Get(mContext, FloatType::Single);
    } else if (type->isDoubleTy()) {
        return FloatType::Get(mContext, FloatType::Double);
    } else if (type->isFP128Ty()) {
        return FloatType::Get(mContext, FloatType::Quad);
    } else if (type->isPointerTy()) {
        //return mMemoryModel.getTypeFromPointerType(llvm::cast<llvm::PointerType>(type));
    }

    llvm::errs() << "Unsupported LLVM Type: " << *type << "\n";
    assert(false && "Unsupported LLVM type.");
}

gazer::Type& ModuleToCfa::typeFromLLVMType(const llvm::Value* value)
{
    return typeFromLLVMType(value->getType());
}



// LLVM pass implementation
//-----------------------------------------------------------------------------

char ModuleToAutomataPass::ID;

void ModuleToAutomataPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<llvm::CallGraphWrapperPass>();
    au.addRequired<llvm::LoopInfoWrapperPass>();
    au.addRequired<llvm::DominatorTreeWrapperPass>();
}

bool ModuleToAutomataPass::runOnModule(llvm::Module& module)
{
    ModuleToCfa::LoopInfoMapTy loops;
    for (Function& function : module) {
        if (!function.isDeclaration()) {
            loops[&function] = &getAnalysis<LoopInfoWrapperPass>(function).getLoopInfo();
        }
    }

    CallGraph& cg = getAnalysis<CallGraphWrapperPass>().getCallGraph();

    GazerContext context;

    ModuleToCfa transformer(module, cg, loops, context);

    auto system = transformer.generate();

    for (Cfa& cfa : *system) {
        llvm::errs() << cfa.getName() << "("
            << llvm::join(to_string_range(cfa.inputs()), ",")
            << ")\n -> "
            << llvm::join(to_string_range(cfa.outputs()), ", ")
            << " {\n"
            << llvm::join(to_string_range(cfa.locals()), "\n")
            << "\n}";
        llvm::errs() << "\n";
    }

    for (Cfa& cfa : *system) {
        cfa.view();
    }

    return false;
}


#if 0
namespace gazer
{


class ModuleToCfa final
{
public:
    ModuleToCfa(llvm::Module& module, GazerContext& context)
        : mModule(module), mContext(context), mExprBuilder(CreateFoldingExprBuilder(context))
    {
        mSystem = std::make_unique<AutomataSystem>(context);
    }

private:
    ExprPtr visitBinaryOperator(llvm::BinaryOperator& binop);
    ExprPtr visitSelectInst(llvm::SelectInst& select);
    ExprPtr visitICmpInst(llvm::ICmpInst& icmp);
    ExprPtr visitFCmpInst(llvm::FCmpInst& fcmp);
    ExprPtr visitCastInst(llvm::CastInst& cast);
    ExprPtr visitCallInst(llvm::CallInst& call);


    Variable* getVariable(const llvm::Value* value);
    ExprPtr operand(const llvm::Value* value);

    ExprPtr asBool(ExprPtr operand);
    ExprPtr asInt(ExprPtr operand, unsigned bits);
    ExprPtr castResult(ExprPtr expr, const Type& type);

    ExprPtr integerCast(llvm::CastInst& cast, ExprPtr operand, unsigned width);

    Type& typeFromLLVMType(const llvm::Type* type);
    Type& typeFromLLVMType(const llvm::Value* value);

private:
    llvm::Module& mModule;
    GazerContext& mContext;
    std::unique_ptr<AutomataSystem> mSystem;
    std::unique_ptr<ExprBuilder> mExprBuilder;
};

} // end namespace gazer

using namespace gazer;
using namespace llvm;

// Utilities
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

static ExprPtr tryToEliminate(const llvm::Instruction& inst, ExprPtr expr)
{
    if (inst.getNumUses() != 1) {
        return nullptr;
    }

    // FCmp instructions will have multiple uses as an expression
    // for a single value, due to their NaN checks.
    // With -assume-no-nan this is no longer the case.
    //if (!AssumeNoNaN && isa<FCmpInst>(*inst.user_begin())) {
    //    return nullptr;
    //}

    auto nonNullary = dyn_cast<NonNullaryExpr>(expr.get());
    if (nonNullary == nullptr || nonNullary->getNumOperands() != 2) {
        return nullptr;
    }

    if (nonNullary->getKind() != Expr::Eq && nonNullary->getKind() != Expr::FEq) {
        return nullptr;
    }

    if (nonNullary->getOperand(0)->getKind() != Expr::VarRef) {
        return nullptr;
    }

    return nonNullary->getOperand(1);
}

gazer::Type& ModuleToCfa::typeFromLLVMType(const llvm::Type* type)
{
    if (type->isIntegerTy()) {
        auto width = type->getIntegerBitWidth();
        if (width == 1) {
            return BoolType::Get(mContext);
        }

        return BvType::Get(mContext, width);
    } else if (type->isHalfTy()) {
        return FloatType::Get(mContext, FloatType::Half);
    } else if (type->isFloatTy()) {
        return FloatType::Get(mContext, FloatType::Single);
    } else if (type->isDoubleTy()) {
        return FloatType::Get(mContext, FloatType::Double);
    } else if (type->isFP128Ty()) {
        return FloatType::Get(mContext, FloatType::Quad);
    } else if (type->isPointerTy()) {
        return mMemoryModel.getTypeFromPointerType(llvm::cast<llvm::PointerType>(type));
    }

    assert(false && "Unsupported LLVM type.");
}

// Basic instruction types
//-----------------------------------------------------------------------------

ExprPtr ModuleToCfa::visitBinaryOperator(llvm::BinaryOperator &binop)
{
    Variable* variable = getVariable(&binop);
    ExprPtr lhs = operand(binop.getOperand(0));
    ExprPtr rhs = operand(binop.getOperand(1));

    auto opcode = binop.getOpcode();
    if (isLogicInstruction(opcode)) {
        ExprPtr expr;
        if (binop.getType()->isIntegerTy(1)) {
            auto boolLHS = asBool(lhs);
            auto boolRHS = asBool(rhs);

            if (binop.getOpcode() == Instruction::And) {
                expr = mExprBuilder.And(boolLHS, boolRHS);
            } else if (binop.getOpcode() == Instruction::Or) {
                expr = mExprBuilder.Or(boolLHS, boolRHS);
            } else if (binop.getOpcode() == Instruction::Xor) {
                expr = mExprBuilder.Xor(boolLHS, boolRHS);
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
                expr = mExprBuilder.BAnd(intLHS, intRHS);
            } else if (binop.getOpcode() == Instruction::Or) {
                expr = mExprBuilder.BOr(intLHS, intRHS);
            } else if (binop.getOpcode() == Instruction::Xor) {
                expr = mExprBuilder.BXor(intLHS, intRHS);
            } else {
                llvm_unreachable("Unknown logic instruction opcode");
            }
        }

        return mExprBuilder.Eq(variable->getRefExpr(), expr);
    } else if (isFloatInstruction(opcode)) {
        ExprPtr expr;
        switch (binop.getOpcode()) {
            case Instruction::FAdd:
                expr = mExprBuilder.FAdd(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
                break;
            case Instruction::FSub:
                expr = mExprBuilder.FSub(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
                break;
            case Instruction::FMul:
                expr = mExprBuilder.FMul(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
                break;
            case Instruction::FDiv:
                expr = mExprBuilder.FDiv(lhs, rhs, llvm::APFloat::rmNearestTiesToEven);
                break;
            default:
                assert(false && "Invalid floating-point operation");
        }

        return mExprBuilder.FEq(variable->getRefExpr(), expr);
    } else {
        auto type = llvm::dyn_cast<gazer::BvType>(&variable->getType());
        assert(type && "Arithmetic results must be bit vector types");

        auto intLHS = asInt(lhs, type->getWidth());
        auto intRHS = asInt(rhs, type->getWidth());

#define HANDLE_INSTCASE(OPCODE, EXPRNAME)                           \
            case OPCODE:                                            \
                expr = mExprBuilder.EXPRNAME(intLHS, intRHS);      \
                break;                                              \

        ExprPtr expr;
        switch (binop.getOpcode()) {
            HANDLE_INSTCASE(Instruction::Add, Add)
            HANDLE_INSTCASE(Instruction::Sub, Sub)
            HANDLE_INSTCASE(Instruction::Mul, Mul)
            HANDLE_INSTCASE(Instruction::SDiv, SDiv)
            HANDLE_INSTCASE(Instruction::UDiv, UDiv)
            HANDLE_INSTCASE(Instruction::SRem, SRem)
            HANDLE_INSTCASE(Instruction::URem, URem)
            HANDLE_INSTCASE(Instruction::Shl, Shl)
            HANDLE_INSTCASE(Instruction::LShr, LShr)
            HANDLE_INSTCASE(Instruction::AShr, AShr)
            default:
                llvm::errs() << binop << "\n";
                assert(false && "Unsupported arithmetic instruction opcode");
        }

#undef HANDLE_INSTCASE

        return mExprBuilder.Eq(variable->getRefExpr(), expr);
    }

    llvm_unreachable("Invalid binary operation kind");
}

ExprPtr ModuleToCfa::visitSelectInst(llvm::SelectInst& select)
{
    Variable* selectVar = getVariable(&select);
    const gazer::Type& type = selectVar->getType();

    auto cond = asBool(operand(select.getCondition()));
    auto then = castResult(operand(select.getTrueValue()), type);
    auto elze = castResult(operand(select.getFalseValue()), type);

    return mExprBuilder.Eq(selectVar->getRefExpr(), mExprBuilder.Select(cond, then, elze));
}

ExprPtr ModuleToCfa::visitICmpInst(llvm::ICmpInst& icmp)
{
    using llvm::CmpInst;

    auto icmpVar = getVariable(&icmp);
    auto lhs = operand(icmp.getOperand(0));
    auto rhs = operand(icmp.getOperand(1));

    auto pred = icmp.getPredicate();

#define HANDLE_PREDICATE(PREDNAME, EXPRNAME)                \
        case PREDNAME:                                          \
            expr = mExprBuilder.EXPRNAME(lhs, rhs);            \
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

    return mExprBuilder.Eq(icmpVar->getRefExpr(), expr);
}

ExprPtr ModuleToCfa::visitFCmpInst(llvm::FCmpInst& fcmp)
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

    return mExprBuilder.Eq(fcmpVar->getRefExpr(), expr);
}

ExprPtr ModuleToCfa::visitCastInst(llvm::CastInst& cast)
{
    auto castOp = operand(cast.getOperand(0));

    if (castOp->getType().isBoolType()) {
        return integerCast(cast, castOp, 1);
    } else if (castOp->getType().isIntType()) {
        return integerCast(
            cast, castOp, dyn_cast<gazer::BvType>(&castOp->getType())->getWidth()
        );
    }

    if (cast.getOpcode() == Instruction::BitCast) {
        auto castTy = cast.getType();
        // Bitcast is no-op, just changes the type.
        // For pointer operands, this means a simple pointer cast.
        // if (castOp->getType().isPointerType() && castTy->isPointerTy()) {
        //     return mExprBuilder.PtrCast(
        //         castOp,
        //         *llvm::dyn_cast<PointerType>(&TypeFromLLVMType(castTy))
        //     );
        // }
    }

    assert(false && "Unsupported cast operation");
}

// Casting and other utilities
//-----------------------------------------------------------------------------

ExprPtr ModuleToCfa::integerCast(llvm::CastInst& cast, ExprPtr operand, unsigned width)
{
    auto variable = getVariable(&cast);
    auto intTy = llvm::dyn_cast<gazer::BvType>(&variable->getType());

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

    return mExprBuilder.Eq(variable->getRefExpr(), castOp);
}
ExprPtr ModuleToCfa::operand(const Value* value)
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
        return FloatLiteralExpr::Get(
            *llvm::dyn_cast<FloatType>(&typeFromLLVMType(cfp->getType())),
            cfp->getValueAPF()
        );
    } else if (const llvm::ConstantPointerNull* ptr = dyn_cast<llvm::ConstantPointerNull>(value)) {
        return mMemoryModel.getNullPointer();
    } else if (isNonConstValue(value)) {
        auto result = mEliminatedValues.find(value);
        if (result != mEliminatedValues.end()) {
            return result->second;
        }

        return getVariable(value)->getRefExpr();
    } else if (isa<llvm::UndefValue>(value)) {
        return mExprBuilder.Undef(typeFromLLVMType(value));
    } else {
        LLVM_DEBUG(llvm::dbgs() << "  Unhandled value for operand: " << *value << "\n");
        assert(false && "Unhandled value type");
    }
}

Variable* ModuleToCfa::getVariable(const Value* value)
{
    auto result = mVariables.find(value);
    assert(result != mVariables.end() && "Variables should be present in the variable map.");

    return result->second;
}

ExprPtr ModuleToCfa::asBool(ExprPtr operand)
{
    if (operand->getType().isBoolType()) {
        return operand;
    } else if (operand->getType().isBvType()) {
        const BvType* bvTy = dyn_cast<BvType>(&operand->getType());
        unsigned bits = bvTy->getWidth();

        return mExprBuilder.Select(
            mExprBuilder.Eq(operand, mExprBuilder.BvLit(0, bits)),
            mExprBuilder.False(),
            mExprBuilder.True()
        );
    } else {
        assert(false && "Unsupported gazer type.");
    }
}

ExprPtr ModuleToCfa::asInt(ExprPtr operand, unsigned bits)
{
    if (operand->getType().isBoolType()) {
        return mExprBuilder.Select(
            operand,
            mExprBuilder.BvLit(1, bits),
            mExprBuilder.BvLit(0, bits)
        );
    } else if (operand->getType().isBvType()) {
        return operand;
    } else {
        assert(false && "Unsupported gazer type.");
    }
}

ExprPtr ModuleToCfa::castResult(ExprPtr expr, const Type& type)
{
    if (type.isBoolType()) {
        return asBool(expr);
    } else if (type.isBvType()) {
        return asInt(expr, dyn_cast<BvType>(&type)->getWidth());
    } else {
        assert(!"Invalid cast result type");
    }
}

#endif