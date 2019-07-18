#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/Automaton/Cfa.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/ADT/StringUtils.h"
#include "gazer/LLVM/Instrumentation/Check.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Debug.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Dominators.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/IR/CallSite.h>

#include "FunctionToCfa.h"

#define DEBUG_TYPE "ModuleToCfa"

using namespace gazer;
using namespace llvm;

namespace gazer {
    cl::opt<bool> NoElimVars("no-elim-vars",
        cl::desc("Do not eliminate temporary variables"));
}

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

template<class Range>
static bool hasUsesInBlockRange(const llvm::Instruction* inst, Range&& range)
{
    for (auto user : inst->users()) {
        if (auto i = llvm::dyn_cast<Instruction>(user)) {
            if (std::find(std::begin(range), std::end(range), i->getParent()) != std::end(range)) {
                return true;
            }
        }
    }

    return false;
}

template<class Range>
static size_t getNumUsesInBlockRange(const llvm::Instruction* inst, Range&& range)
{
    size_t cnt = 0;
    for (auto user : inst->users()) {
        if (auto i = llvm::dyn_cast<Instruction>(user)) {
            if (std::find(std::begin(range), std::end(range), i->getParent()) != std::end(range)) {
                cnt += 1;
            }
        }
    }

    return cnt;
}

static bool isErrorBlock(llvm::BasicBlock* bb)
{
    auto inst = bb->getFirstInsertionPt();
    // In error blocks, the first instruction should be the 'gazer.error_code' call.

    if (auto call = llvm::dyn_cast<CallInst>(inst)) {
        Function* function = call->getCalledFunction();
        if (function != nullptr && function->getName() == "gazer.error_code") {
            return true;
        } 
    }

    return false;
}

std::unique_ptr<AutomataSystem> ModuleToCfa::generate(
    llvm::DenseMap<llvm::Value*, Variable*>& variables,
    llvm::DenseMap<Location*, llvm::BasicBlock*>& blockEntries
) {
    GenerationContext genCtx(*mSystem, mMemoryModel);
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

        Cfa* cfa = mSystem->createCfa(function.getName());

        DenseSet<BasicBlock*> visitedBlocks;

        // Create a CFA for each loop nested in this function
        LoopInfo* loopInfo = mLoops[&function];
        genCtx.LoopInfo = loopInfo;

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

            std::vector<BasicBlock*> loopOnlyBlocks;
            std::copy_if(
                loopBlocks.begin(), loopBlocks.end(),
                std::back_inserter(loopOnlyBlocks),
                [&visitedBlocks] (BasicBlock* b) { return visitedBlocks.count(b) == 0; }
            );

            // Insert the loop variables
            for (BasicBlock* bb : loopBlocks) {
                for (Instruction& inst : *bb) {
                    Variable* variable = nullptr;

                    if (inst.getOpcode() == Instruction::PHI && bb == loop->getHeader()) {
                        // PHI nodes of the entry block should also be inputs.
                        variable = nested->createInput(inst.getName(), mMemoryModel.translateType(inst.getType()));
                        loopGenInfo.PhiInputs[&inst] = variable;
                        variables[&inst] = variable;
                    } else {
                        // Add operands which were defined in the caller as inputs
                        for (auto oi = inst.op_begin(), oe = inst.op_end(); oi != oe; ++oi) {
                            llvm::Value* value = *oi;
                            if (isDefinedInCaller(value, loopBlocks) && loopGenInfo.Inputs.count(value) == 0) {
                                auto argVariable = nested->createInput(
                                    value->getName(),
                                    mMemoryModel.translateType(value->getType())
                                );
                                loopGenInfo.Inputs[value] = argVariable;

                                LLVM_DEBUG(llvm::dbgs() << "  Added input variable " << *argVariable << "\n");
                            }
                        }

                        // Do not create a variable if the instruction has no return type.
                        if (inst.getType()->isVoidTy()) {
                            continue;
                        }

                        // If the instruction is defined in an already visited block, it is a local of
                        // a subloop rather than this loop.
                        if (visitedBlocks.count(bb) == 0 || hasUsesInBlockRange(&inst, loopOnlyBlocks)) {
                            variable = nested->createLocal(inst.getName(), mMemoryModel.translateType(inst.getType()));
                            loopGenInfo.Locals[&inst] = variable;
                            variables[&inst] = variable;

                            LLVM_DEBUG(llvm::dbgs() << "  Added local variable " << *variable << "\n");
                        } else {
                            // TODO
                            variable = nullptr;
                        }
                    }


                    for (auto user : inst.users()) {
                        if (auto i = llvm::dyn_cast<Instruction>(user)) {
                            if (std::find(loopBlocks.begin(), loopBlocks.end(), i->getParent()) ==
                                loopBlocks.end()) {
                                nested->addOutput(variable);
                                loopGenInfo.Outputs[&inst] = variable;

                                LLVM_DEBUG(llvm::dbgs() << "  Added output variable " << *variable << "\n");
                                break;
                            }
                        }
                    }
                }

                // Create locations for this block
                if (visitedBlocks.count(bb)  == 0) {
                    Location* entry = nested->createLocation();
                    Location* exit = isErrorBlock(bb) ? nested->createErrorLocation() : nested->createLocation();

                    loopGenInfo.Blocks[bb] = std::make_pair(entry, exit);
                }
            }

            // If the loop has multiple exits, add a selector output to disambiguate between these.
            llvm::SmallVector<llvm::BasicBlock*, 4> exitBlocks;
            loop->getUniqueExitBlocks(exitBlocks);
            if (exitBlocks.size() != 1) {
                loopGenInfo.ExitVariable = nested->createLocal(LoopOutputSelectorName, BvType::Get(mContext, 8));
                nested->addOutput(loopGenInfo.ExitVariable);
                for (size_t i = 0; i < exitBlocks.size(); ++i) {
                    loopGenInfo.ExitBlocks[exitBlocks[i]] = exprBuilder->BvLit(i, 8);
                }
            }

            std::vector<BasicBlock*> blocksToEncode;
            std::copy_if(
                loopBlocks.begin(), loopBlocks.end(),
                std::back_inserter(blocksToEncode),
                [&visitedBlocks] (BasicBlock* b) { return visitedBlocks.count(b) == 0; }
            );

            visitedBlocks.insert(loop->getBlocks().begin(), loop->getBlocks().end());

            // Do the actual encoding.
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
            Variable* variable = cfa->createInput(argument.getName(), mMemoryModel.translateType(argument.getType()));
            genInfo.Inputs[&argument] = variable;
            variables[&argument] = variable;
        }

        // TODO: Maybe add RET_VAL to genInfo outputs in some way?
        if (!function.getReturnType()->isVoidTy()) {
            auto retval = cfa->createLocal(FunctionReturnValueName, mMemoryModel.translateType(function.getReturnType()));
            cfa->addOutput(retval);
        }

        // At this point, the loops are already encoded, we only need to handle the blocks outside of the loops.
        std::vector<BasicBlock*> functionBlocks;
        std::for_each(function.begin(), function.end(), [&visitedBlocks, &functionBlocks] (BasicBlock& bb) {
            if (visitedBlocks.count(&bb) == 0) {
                functionBlocks.push_back(&bb);
            }
        });

        // For the local variables, we only need to add the values not present in any of the loops.
        for (BasicBlock& bb : function) {
            for (Instruction& inst : bb) {
                if (loopInfo->getLoopFor(&bb) != nullptr && !hasUsesInBlockRange(&inst, functionBlocks)) {
                    LLVM_DEBUG(llvm::dbgs() << "Skipped " << inst << "\n");
                    continue;
                }

                // FIXME: This requires the instnamer pass as a dependency, we should find another way around this.
                if (inst.getName() != "") {
                    Variable* variable = cfa->createLocal(inst.getName(), mMemoryModel.translateType(inst.getType()));
                    genInfo.Locals[&inst] = variable;
                    variables[&inst] = variable;
                }
            }
        }

        for (BasicBlock* bb : functionBlocks) {
            Location* entry = cfa->createLocation();
            Location* exit = isErrorBlock(bb) ? cfa->createErrorLocation() : cfa->createLocation();

            genInfo.Blocks[bb] = std::make_pair(entry, exit);
        }

        BlocksToCfa blocksToCfa(
            genCtx,
            genInfo,
            functionBlocks,
            cfa,
            *exprBuilder
        );
        blocksToCfa.encode(&function.getEntryBlock());
    }


    // CFAs must be connected graphs. Remove unreachable components now.
    for (auto& cfa : *mSystem) {
        cfa.removeUnreachableLocations();
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

            if (auto call = llvm::dyn_cast<CallInst>(&inst)) {
                Function* callee = call->getCalledFunction();
                if (callee != nullptr && !callee->isDeclaration()) {
                    auto it = mGenCtx.FunctionMap.find(callee);
                    assert(it != mGenCtx.FunctionMap.end()
                        && "Function definitions must be present in the FunctionMap of the CFA generator!");
                    
                    CfaGenInfo& calledAutomatonInfo = it->second;
                    Cfa* calledCfa = calledAutomatonInfo.Automaton;

                    assert(calledCfa != nullptr && "The callee automaton must exist in a function call!");
                    
                    // Split the current transition here and create a call.
                    Location* callBegin = mCfa->createLocation();
                    Location* callEnd = mCfa->createLocation();

                    mCfa->createAssignTransition(entry, callBegin, assignments);

                    std::vector<ExprPtr> inputs;
                    std::vector<VariableAssignment> outputs;

                    for (size_t i = 0; i < call->getNumArgOperands(); ++i) {
                        ExprPtr expr = this->operand(call->getArgOperand(i));
                        inputs.push_back(expr);
                    }

                    if (!callee->getReturnType()->isVoidTy()) {
                        Variable* variable = getVariable(&inst);

                        // Find the return variable of this function.                        
                        Variable* retval = calledCfa->findOutputByName(ModuleToCfa::FunctionReturnValueName);
                        assert(retval != nullptr && "A non-void function must have a return value!");
                        
                        outputs.emplace_back(variable, retval->getRefExpr());
                    }

                    mCfa->createCallTransition(callBegin, callEnd, calledCfa, inputs, outputs);

                    // Continue the translation from the end location of the call.
                    entry = callEnd;

                    // Do not generate an assignment for this call.
                    continue;
                }
            }

            if (inst.getType()->isVoidTy()) {
                continue;
            }

            Variable* variable = getVariable(&inst);
            ExprPtr expr = this->transform(inst);

            if (!NoElimVars
                && inst.getOpcode() != Instruction::Call
                && getNumUsesInBlockRange(&inst, mBlocks) == 1
            ) {
                mEliminatedVars[&*it] = expr;
            } else {
                assignments.emplace_back(variable, expr);
            }
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

                    // Add possible calls arguments.
                    // Due to the SSA-formed LLVM IR, regular inputs are not modified by loop iterations.
                    // For PHI inputs, we need to determine which parent block to use for expression translation.
                    ExprVector loopArgs(mCfa->getNumInputs());
                    for (auto entry : mGenInfo.PhiInputs) {
                        auto incoming = llvm::cast<PHINode>(entry.first)->getIncomingValueForBlock(bb);
                        size_t idx = mCfa->getInputNumber(entry.second);

                        loopArgs[idx] = operand(incoming);
                    }

                    for (auto entry : mGenInfo.Inputs) {
                        size_t idx = mCfa->getInputNumber(entry.second);
                        loopArgs[idx] = entry.second->getRefExpr();
                    }

                    Variable* exitSelector = nullptr;
                    std::vector<VariableAssignment> outputArgs;                    
                    this->insertOutputAssignments(mGenInfo, outputArgs);

                    if (mGenInfo.ExitVariable != nullptr) {
                        outputArgs.emplace_back(mGenInfo.ExitVariable, mGenInfo.ExitVariable->getRefExpr());
                    }

                    mCfa->createCallTransition(loc, mCfa->getExit(), mCfa, loopArgs, outputArgs);
                } else if (std::find(mBlocks.begin(), mBlocks.end(), succ) != mBlocks.end()) {
                    // Else if the target is is inside the block region, just create a simple edge.
                    Location* to = mGenInfo.Blocks[succ].first;

                    std::vector<VariableAssignment> phiAssignments;
                    insertPhiAssignments(bb, succ, phiAssignments);

                    mCfa->createAssignTransition(exit, to, succCondition, phiAssignments);
                } else if (auto loop = mGenCtx.LoopInfo->getLoopFor(succ)) {
                    // If this is a nested loop, create a call to the corresponding automaton.
                    CfaGenInfo& nestedLoopInfo = mGenCtx.LoopMap[loop];
                    auto nestedCfa = nestedLoopInfo.Automaton;

                    ExprVector loopArgs(nestedCfa->getNumInputs());
                    for (auto entry : nestedLoopInfo.PhiInputs) {
                        auto incoming = llvm::cast<PHINode>(entry.first)->getIncomingValueForBlock(bb);
                        size_t idx = nestedCfa->getInputNumber(entry.second);

                        loopArgs[idx] = operand(incoming);
                    }

                    for (auto entry : nestedLoopInfo.Inputs) {
                        // Whatever variable is used as an input, it should be present here as well in some form.
                        Variable* variable = getVariable(entry.first);
                        size_t idx = nestedCfa->getInputNumber(entry.second);

                        loopArgs[idx] = variable->getRefExpr();
                    }

                    std::vector<VariableAssignment> outputArgs;
                    this->insertOutputAssignments(nestedLoopInfo, outputArgs);

                    Variable* exitSelector = nullptr;
                    if (nestedLoopInfo.ExitVariable != nullptr) {
                        exitSelector = mCfa->createLocal(
                            Twine(ModuleToCfa::LoopOutputSelectorName).concat(Twine(mCounter++)).str(),
                            BvType::Get(mGenCtx.System.getContext(), 8)
                        );
                        outputArgs.emplace_back(exitSelector, nestedLoopInfo.ExitVariable->getRefExpr());
                    }

                    auto loc = mCfa->createLocation();
                    mCfa->createCallTransition(exit, loc, succCondition, nestedCfa, loopArgs, outputArgs);

                    llvm::SmallVector<BasicBlock*, 4> exitBlocks;
                    loop->getUniqueExitBlocks(exitBlocks);

                    for (BasicBlock* exitBlock : exitBlocks) {
                        auto result = mGenInfo.Blocks.find(exitBlock);
                        if (result != mGenInfo.Blocks.end()) {
                            mCfa->createAssignTransition(loc, result->second.first, getExitCondition(exitBlock, exitSelector, nestedLoopInfo));
                        } else {
                            this->createExitTransition(succ, exit, succCondition);
                        }
                    }
                } else {
                    this->createExitTransition(succ, exit, succCondition);
                }
            }
        } else if (auto ret = llvm::dyn_cast<ReturnInst>(terminator)) {
            if (ret->getReturnValue() == nullptr) {
                mCfa->createAssignTransition(exit, mCfa->getExit(), mExprBuilder.True());
            } else {
                Variable* retval = mCfa->findOutputByName(ModuleToCfa::FunctionReturnValueName);
                mCfa->createAssignTransition(exit, mCfa->getExit(), mExprBuilder.True(), {
                    VariableAssignment{ retval, operand(ret->getReturnValue()) }
                });
            }
        } else if (terminator->getOpcode() == Instruction::Unreachable) {
            // Do nothing.
        } else {
            LLVM_DEBUG(llvm::dbgs() << *terminator << "\n");
            llvm_unreachable("Unknown terminator instruction.");
        }
    }
}

void BlocksToCfa::createExitTransition(BasicBlock* target, Location* pred, ExprPtr succCondition)
{
    // If the target is outside of our region, create a simple edge to the exit.
    std::vector<VariableAssignment> exitAssigns;

    if (mGenInfo.ExitVariable != nullptr) {
        // If there are multiple exits, create an assignment to indicate which one to take.
        ExprPtr exitVal = mGenInfo.ExitBlocks[target];
        assert(exitVal != nullptr && "An exit block must be present in the exit blocks map!");

        exitAssigns.emplace_back(mGenInfo.ExitVariable, exitVal);
    }

    mCfa->createAssignTransition(pred, mCfa->getExit(), succCondition, exitAssigns);
}

ExprPtr BlocksToCfa::getExitCondition(llvm::BasicBlock* target, Variable* exitSelector, CfaGenInfo& nestedInfo)
{
    if (nestedInfo.ExitVariable == nullptr) {
        return mExprBuilder.True();
    }

    ExprPtr exitVal = nestedInfo.ExitBlocks[target];
    assert(exitVal != nullptr && "An exit block must be present in the exit blocks map!");

    return mExprBuilder.Eq(exitSelector->getRefExpr(), exitVal);
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
    HANDLE_INST(Instruction::Call, CallInst)
    HANDLE_INST(Instruction::FCmp, FCmpInst)
    HANDLE_INST(Instruction::Select, SelectInst)

#undef HANDLE_INST

    llvm_unreachable("Unsupported instruction kind");
}

void BlocksToCfa::insertOutputAssignments(CfaGenInfo& callee, std::vector<VariableAssignment>& outputArgs)
{
    // For the outputs, find the corresponding variables in the parent and create the assignments.
    for (auto& pair : callee.Outputs) {
        llvm::Value* value = pair.first;
        Variable* nestedOutputVar = pair.second;

        LLVM_DEBUG(
            llvm::dbgs() << "  Inserting output assignment for " << *value
            << " variable " << *nestedOutputVar << "\n"
        );

        // It is either a local or input in parent.
        auto result = mGenInfo.Locals.find(value);
        if (result == mGenInfo.Locals.end()) {
            result = mGenInfo.Inputs.find(value);
            if (result == mGenInfo.Inputs.end()) {
                result = mGenInfo.PhiInputs.find(value);
                assert(result != mGenInfo.PhiInputs.end()
                       && "Nested output variable should be present in parent as an input or local!");
            }
        }

        Variable* parentVar = result->second;
        outputArgs.emplace_back(parentVar, nestedOutputVar->getRefExpr());
    }
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

ExprPtr BlocksToCfa::visitCallInst(llvm::CallInst& call)
{
    gazer::Type& callTy = mGenCtx.TheMemoryModel.translateType(call.getType());

    const Function* callee = call.getCalledFunction();
    if (callee == nullptr) {
        return UndefExpr::Get(callTy);
        // This is an indirect call, use the memory model to resolve it.
        //return mMemoryModel.handleCall(call);
    }

    return UndefExpr::Get(callTy);
}

ExprPtr BlocksToCfa::visitCastInst(llvm::CastInst& cast)
{
    auto castOp = operand(cast.getOperand(0));
    //if (cast.getOperand(0)->getType()->isPointerTy()) {
    //    return mMemoryModel.handlePointerCast(cast, castOp);
    //}

    llvm::errs() << cast << "\n";
    
    if (cast.getType()->isFloatingPointTy()) {
        auto& fltTy = *llvm::dyn_cast<FloatType>(&mGenCtx.TheMemoryModel.translateType(cast.getType()));

        switch (cast.getOpcode()) {
            case Instruction::FPExt:
            case Instruction::FPTrunc:
                llvm::errs() << castOp->getType() << " " << *castOp << "\n";
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
        auto& bvTy = *llvm::dyn_cast<BvType>(&mGenCtx.TheMemoryModel.translateType(cast.getType()));
        return mExprBuilder.FpToSigned(castOp, bvTy, llvm::APFloat::rmNearestTiesToEven);
    } else if (cast.getOpcode() == Instruction::UIToFP) {
        auto& bvTy = *llvm::dyn_cast<BvType>(&mGenCtx.TheMemoryModel.translateType(cast.getType()));
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
    } else if (const llvm::ConstantFP* cfp = dyn_cast<llvm::ConstantFP>(value)) {
        return mExprBuilder.FloatLit(cfp->getValueAPF());
    } /*else if (const llvm::ConstantPointerNull* ptr = dyn_cast<llvm::ConstantPointerNull>(value)) {
        return mMemoryModel.getNullPointer();
    } */ else if (isNonConstValue(value)) {
        auto result = mEliminatedVars.find(value);
        if (result != mEliminatedVars.end()) {
            return result->second;
        }

        return getVariable(value)->getRefExpr();
    } else if (isa<llvm::UndefValue>(value)) {
        return mExprBuilder.Undef(mGenCtx.TheMemoryModel.translateType(value->getType()));
    } else {
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

    result = mGenInfo.PhiInputs.find(value);
    if (result != mGenInfo.PhiInputs.end()) {
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

std::unique_ptr<AutomataSystem> gazer::translateModuleToAutomata(
    llvm::Module& module,
    std::unordered_map<llvm::Function*, llvm::LoopInfo*>& loopInfos,
    GazerContext& context,
    MemoryModel& memoryModel,
    llvm::DenseMap<llvm::Value*, Variable*>& variables,
    llvm::DenseMap<Location*, llvm::BasicBlock*>& blockEntries
) {
    ModuleToCfa transformer(module, loopInfos, context, memoryModel);
    return transformer.generate(variables, blockEntries);
}

// LLVM pass implementation
//-----------------------------------------------------------------------------

char ModuleToAutomataPass::ID;

void ModuleToAutomataPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
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

    DummyMemoryModel memoryModel(mContext);

    mSystem = translateModuleToAutomata(module, loops, mContext, memoryModel, mVariables, mBlocks);
/*
    for (Cfa& cfa : *mSystem) {
        llvm::errs() << cfa.getName() << "("
            << llvm::join(to_string_range(cfa.inputs()), ",")
            << ")\n -> "
            << llvm::join(to_string_range(cfa.outputs()), ", ")
            << " {\n"
            << llvm::join(to_string_range(cfa.locals()), "\n")
            << "\n}";
        llvm::errs() << "\n";
    } */

    return false;
}
