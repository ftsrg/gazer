#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/Automaton/Cfa.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/ADT/StringUtils.h"
#include "gazer/LLVM/Instrumentation/Check.h"
#include "gazer/Core/Expr/ExprUtils.h"

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

namespace gazer
{
    cl::opt<ModuleToAutomataSettings::ElimVarsLevel> ElimVarsLevel("elim-vars", cl::desc("Level for variable elimination:"),
        cl::values(
            clEnumValN(ModuleToAutomataSettings::ElimVars_Off, "off", "Do not eliminate variables"),
            clEnumValN(ModuleToAutomataSettings::ElimVars_Normal, "normal", "Eliminate variables with only one use"),
            clEnumValN(ModuleToAutomataSettings::ElimVars_Aggressive, "aggressive", "Eliminate all eligible variables")
        ),
        cl::init(ModuleToAutomataSettings::ElimVars_Normal)
    );

    cl::opt<bool> NoSimplifyExpr("no-simplify-expr", cl::desc("Do not simplify expessions."));
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
        if (function != nullptr && function->getName() == CheckRegistry::ErrorFunctionName) {
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
    genCtx.Settings = mSettings;
    std::unique_ptr<ExprBuilder> exprBuilder;

    if (!NoSimplifyExpr) {
        exprBuilder = CreateFoldingExprBuilder(mContext);
    } else {
        exprBuilder = CreateExprBuilder(mContext);
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
                        loopGenInfo.addPhiInput(&inst, variable);
                        variables[&inst] = variable;
                        
                        LLVM_DEBUG(llvm::dbgs() << "  Added PHI input variable " << *variable << "\n");
                    } else {
                        // Add operands which were defined in the caller as inputs
                        for (auto oi = inst.op_begin(), oe = inst.op_end(); oi != oe; ++oi) {
                            llvm::Value* value = *oi;
                            if (isDefinedInCaller(value, loopBlocks) && !loopGenInfo.hasInput(value)) {
                                auto argVariable = nested->createInput(
                                    value->getName(),
                                    mMemoryModel.translateType(value->getType())
                                );
                                loopGenInfo.addInput(value, argVariable);

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
                            loopGenInfo.addLocal(&inst, variable);
                            variables[&inst] = variable;

                            LLVM_DEBUG(llvm::dbgs() << "  Added local variable " << *variable << "\n");
                        } else {
                            // TODO
                            variable = nullptr;
                        }
                    }

                    for (auto user : inst.users()) {
                        if (auto i = llvm::dyn_cast<Instruction>(user)) {
                            if (std::find(loopBlocks.begin(), loopBlocks.end(), i->getParent()) == loopBlocks.end()) {
                                std::string name = Twine(variable->getName(), "_out").str();
                                auto copyOfVar = nested->createLocal(name, variable->getType());

                                nested->addOutput(copyOfVar);
                                loopGenInfo.Outputs[&inst] = copyOfVar;
                                loopGenInfo.LoopOutputs[&inst] = VariableAssignment{ copyOfVar, variable->getRefExpr() };

                                LLVM_DEBUG(llvm::dbgs() << "  Added output variable " << *copyOfVar << "\n");
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
                    loopGenInfo.addReverseBlockIfTraceEnabled(bb, entry);
                    loopGenInfo.addReverseBlockIfTraceEnabled(bb, exit);
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
            genInfo.addInput(&argument, variable);
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
                if (auto loop = loopInfo->getLoopFor(&bb)) {
                    // If the variable is an output of a loop, add it here as a local variable
                    Variable* output = genCtx.LoopMap[loop].findOutput(&inst);
                    if (output == nullptr && !hasUsesInBlockRange(&inst, functionBlocks)) {
                        LLVM_DEBUG(llvm::dbgs() << "Skipped " << inst << "\n");
                        continue;
                    }
                }

                // FIXME: This requires the instnamer pass as a dependency, we should find another way around this.
                if (inst.getName() != "") {
                    Variable* variable = cfa->createLocal(inst.getName(), mMemoryModel.translateType(inst.getType()));
                    genInfo.addLocal(&inst, variable);
                    variables[&inst] = variable;
                }
            }
        }

        for (BasicBlock* bb : functionBlocks) {
            Location* entry = cfa->createLocation();
            Location* exit = isErrorBlock(bb) ? cfa->createErrorLocation() : cfa->createLocation();

            genInfo.Blocks[bb] = std::make_pair(entry, exit);
            genInfo.addReverseBlockIfTraceEnabled(bb, entry);
            genInfo.addReverseBlockIfTraceEnabled(bb, exit);
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
                if (callee == nullptr) {
                    // We do not support indirect calls yet.
                    continue;
                } else if (!callee->isDeclaration()) {
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
                } else if (callee->getName() == CheckRegistry::ErrorFunctionName) {
                    assert(exit->isError() && "The target location of a 'gazer.error_code' call must be an error location!");
                    llvm::Value* arg = call->getArgOperand(0);

                    mCfa->addErrorCode(exit, operand(arg));
                }
            } else if (auto store = llvm::dyn_cast<StoreInst>(&inst)) {
                auto storeValue = this->operand(store->getValueOperand());
                auto ptr = this->operand(store->getPointerOperand());

                auto storeRes = mGenCtx.TheMemoryModel.handleStore(*store, ptr, storeValue);
                if (storeRes.has_value()) {
                    assignments.push_back(*storeRes);
                }

                continue;
            }

            if (inst.getType()->isVoidTy()) {
                continue;
            }

            Variable* variable = getVariable(&inst);
            ExprPtr expr = this->transform(inst);

            if (!tryToEliminate(inst, expr)) {
                assignments.emplace_back(variable, expr);
            } else {
                mEliminatedVarsSet.insert(variable);
            }
        }

        mCfa->createAssignTransition(entry, exit, assignments);

        // Handle the outgoing edges
        auto terminator = bb->getTerminator();

        if (auto br = llvm::dyn_cast<BranchInst>(terminator)) {
            ExprPtr condition = br->isConditional() ? operand(br->getCondition()) : mExprBuilder.True();

            for (unsigned succIdx = 0; succIdx < br->getNumSuccessors(); ++succIdx) {
                BasicBlock* succ = br->getSuccessor(succIdx);
                ExprPtr succCondition = succIdx == 0 ? condition : mExprBuilder.Not(condition);

                handleSuccessor(succ, succCondition, bb, entryBlock, exit);
            }
        } else if (auto swi = llvm::dyn_cast<SwitchInst>(terminator)) {
            ExprPtr condition = operand(swi->getCondition());

            ExprPtr prevConds = mExprBuilder.True();
            for (auto ci = swi->case_begin(); ci != swi->case_end(); ++ci) {
                ExprPtr val = operand(ci->getCaseValue());
                BasicBlock* succ = ci->getCaseSuccessor();

                ExprPtr succCondition = mExprBuilder.And(
                    prevConds,
                    mExprBuilder.Eq(condition, val)
                );
                prevConds = mExprBuilder.And(prevConds, mExprBuilder.NotEq(condition, val));

                handleSuccessor(succ, succCondition, bb, entryBlock, exit);
            }

            handleSuccessor(swi->getDefaultDest(), prevConds, bb, entryBlock, exit);
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

    // Do a clean-up, remove eliminated variables from the CFA.
    if (!mGenCtx.Settings.isElimVarsOff()) {
        mCfa->removeLocalsIf([this](Variable* v) {
            return mEliminatedVarsSet.count(v) != 0;
        });
    }
}

bool BlocksToCfa::tryToEliminate(Instruction& inst, ExprPtr expr)
{
    if (mGenCtx.Settings.isElimVarsOff()) {
        return false;
    }

    // Never eliminate variables obtained from call instructions,
    // as they might be needed to obtain a counterexample.
    if (inst.getOpcode() == Instruction::Call) {
        return false;
    }

    // Do not eliminate variables which are loop outputs, as these will be needed
    // for the output assignments.
    if (mGenInfo.LoopOutputs.count(&inst) != 0) {
        return false;
    }

    // On 'Normal' level, we do not want to inline expressions which have multiple uses and have already inlined operands.
    if (
        !mGenCtx.Settings.isElimVarsAggressive() &&
        getNumUsesInBlockRange(&inst, mBlocks) > 1 &&
        std::any_of(inst.op_begin(), inst.op_end(), [this](llvm::Use& op) {
            llvm::Value* v = &*op;
            return llvm::isa<Instruction>(v) && mInlinedVars.count(llvm::cast<Instruction>(v)) != 0;
        })
    ) {
        return false;
    }

    mInlinedVars[&inst] = expr;
    return true;
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

    // Add the possible loop exit assignments
    for (auto& entry : mGenInfo.LoopOutputs) {
        VariableAssignment& assign = entry.second;
        exitAssigns.push_back(assign);
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

void BlocksToCfa::handleSuccessor(BasicBlock* succ, ExprPtr& succCondition, BasicBlock* parent,
    BasicBlock* entryBlock, Location* exit)
{
    if (succ == entryBlock) {
        // If the target is the loop header (entry block), create a call to this same automaton.
        auto loc = mCfa->createLocation();

        mCfa->createAssignTransition(exit, loc, succCondition);

        // Add possible calls arguments.
        // Due to the SSA-formed LLVM IR, regular inputs are not modified by loop iterations.
        // For PHI inputs, we need to determine which parent block to use for expression translation.
        ExprVector loopArgs(mCfa->getNumInputs());
        for (auto entry : mGenInfo.PhiInputs) {
            auto incoming = cast<PHINode>(entry.first)->getIncomingValueForBlock(parent);
            size_t idx = mCfa->getInputNumber(entry.second);

            loopArgs[idx] = operand(incoming);
        }

        for (auto entry : mGenInfo.Inputs) {
            size_t idx = mCfa->getInputNumber(entry.second);
            loopArgs[idx] = entry.second->getRefExpr();
        }

        std::vector<VariableAssignment> outputArgs;
        insertOutputAssignments(mGenInfo, outputArgs);

        if (mGenInfo.ExitVariable != nullptr) {
            outputArgs.emplace_back(mGenInfo.ExitVariable, mGenInfo.ExitVariable->getRefExpr());
        }

        mCfa->createCallTransition(loc, mCfa->getExit(), mCfa, loopArgs, outputArgs);
    } else if (std::find(mBlocks.begin(), mBlocks.end(), succ) != mBlocks.end()) {
        // Else if the target is is inside the block region, just create a simple edge.
        Location* to = mGenInfo.Blocks[succ].first;

        std::vector<VariableAssignment> phiAssignments;
        insertPhiAssignments(parent, succ, phiAssignments);

        mCfa->createAssignTransition(exit, to, succCondition, phiAssignments);
    } else if (auto loop = mGenCtx.LoopInfo->getLoopFor(succ)) {
        // If this is a nested loop, create a call to the corresponding automaton.
        CfaGenInfo& nestedLoopInfo = mGenCtx.LoopMap[loop];
        auto nestedCfa = nestedLoopInfo.Automaton;

        ExprVector loopArgs(nestedCfa->getNumInputs());
        for (auto entry : nestedLoopInfo.PhiInputs) {
            auto incoming = cast<PHINode>(entry.first)->getIncomingValueForBlock(parent);
            size_t idx = nestedCfa->getInputNumber(entry.second);

            loopArgs[idx] = operand(incoming);
        }

        for (auto entry : nestedLoopInfo.Inputs) {
            // Whatever variable is used as an input, it should be present here as well in some form.
            size_t idx = nestedCfa->getInputNumber(entry.second);

            loopArgs[idx] = operand(entry.first);
        }

        std::vector<VariableAssignment> outputArgs;
        insertOutputAssignments(nestedLoopInfo, outputArgs);

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

        SmallVector<BasicBlock*, 4> exitBlocks;
        loop->getUniqueExitBlocks(exitBlocks);

        for (BasicBlock* exitBlock : exitBlocks) {
            auto result = mGenInfo.Blocks.find(exitBlock);
            if (result != mGenInfo.Blocks.end()) {
                mCfa->createAssignTransition(
                    loc, result->second.first,
                    getExitCondition(exitBlock, exitSelector, nestedLoopInfo)
                );
            } else {
                createExitTransition(succ, exit, succCondition);
            }
        }
    } else {
        createExitTransition(succ, exit, succCondition);
    }
}

void BlocksToCfa::insertOutputAssignments(CfaGenInfo& callee, std::vector<VariableAssignment>& outputArgs)
{
    // For the outputs, find the corresponding variables in the parent and create the assignments.
    for (auto& pair : callee.Outputs) {
        const llvm::Value* value = pair.first;
        Variable* nestedOutputVar = pair.second;

        LLVM_DEBUG(
            llvm::dbgs() << "  Inserting output assignment for " << *value
            << " variable " << *nestedOutputVar << "\n"
        );

        // It is either a local or input in parent.
        Variable* parentVar;

        // If the value is a loop output, we must use its "_out" variable.
        auto loopVar = mGenInfo.LoopOutputs.find(value);
        if (loopVar != mGenInfo.LoopOutputs.end()) {
            parentVar = loopVar->second.getVariable();
        } else {
            parentVar = mGenInfo.findVariable(value);
        }

        assert(parentVar != nullptr && "Nested output variable should be present in parent as an input or local!");

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

ExprPtr BlocksToCfa::lookupInlinedVariable(const llvm::Value* value)
{
    return mInlinedVars.lookup(value);
}

Variable* BlocksToCfa::getVariable(const Value* value)
{
    LLVM_DEBUG(llvm::dbgs() << "Getting variable for value " << *value << "\n");

    auto result = mGenInfo.findVariable(value);
    assert(result != nullptr && "Variables should be present in one of the variable maps!");

    return result;
}

std::unique_ptr<AutomataSystem> gazer::translateModuleToAutomata(
    llvm::Module& module,
    ModuleToAutomataSettings settings,
    std::unordered_map<llvm::Function*, llvm::LoopInfo*>& loopInfos,
    GazerContext& context,
    MemoryModel& memoryModel,
    llvm::DenseMap<llvm::Value*, Variable*>& variables,
    llvm::DenseMap<Location*, llvm::BasicBlock*>& blockEntries
) {
    ModuleToCfa transformer(module, loopInfos, context, memoryModel, settings);
    return transformer.generate(variables, blockEntries);
}

// LLVM pass implementation
//-----------------------------------------------------------------------------

char ModuleToAutomataPass::ID;

void ModuleToAutomataPass::getAnalysisUsage(llvm::AnalysisUsage& au) const
{
    au.addRequired<llvm::LoopInfoWrapperPass>();
    au.addRequired<llvm::DominatorTreeWrapperPass>();
    au.setPreservesAll();
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

    ModuleToAutomataSettings settings;
    settings.setElimVarsLevel(ElimVarsLevel);

    llvm::outs() << "Translating module.\n";
    mSystem = translateModuleToAutomata(module, settings, loops, mContext, memoryModel, mVariables, mBlocks);

    // for (Cfa& cfa : *mSystem) {
    //     llvm::errs() << cfa.getName() << "("
    //         << llvm::join(to_string_range(cfa.inputs()), ",")
    //         << ")\n -> "
    //         << llvm::join(to_string_range(cfa.outputs()), ", ")
    //         << " {\n"
    //         << llvm::join(to_string_range(cfa.locals()), "\n")
    //         << "\n}";
    //     llvm::errs() << "\n";
    // }

    return false;
}
