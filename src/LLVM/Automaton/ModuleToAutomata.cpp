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
/// \file This file contains the implementation of the Module to CFA
/// transformation.
///
//===----------------------------------------------------------------------===//
#include "FunctionToCfa.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"

#include "gazer/LLVM/Memory/MemoryModel.h"
#include "gazer/LLVM/Instrumentation/Check.h"

#include "gazer/Automaton/Cfa.h"
#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Core/Expr/ExprUtils.h"

#include "gazer/ADT/StringUtils.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/Debug.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/ADT/StringExtras.h>

#define DEBUG_TYPE "ModuleToCfa"

using namespace gazer;
using namespace gazer::llvm2cfa;
using namespace llvm;

static bool isDefinedInCaller(llvm::Value* value, llvm::ArrayRef<llvm::BasicBlock*> blocks)
{
    if (isa<Argument>(value)) {
        return true;
    }

    if (auto i = dyn_cast<Instruction>(value)) {
        if (std::find(blocks.begin(), blocks.end(), i->getParent()) != blocks.end()) {
            return false; // NOLINT
        }

        return true;
    }

    // TODO: Is this always correct?
    return false;
}

template<class AccessKind, class Range>
static void memoryAccessOfKind(Range& range, llvm::SmallVectorImpl<AccessKind*>& vec)
{
    static_assert(std::is_base_of_v<MemoryAccess, AccessKind>, "AccessKind must be a subclass of MemoryAccess!");

    llvm::for_each(classof_range<AccessKind>(range), [&vec](auto& v) {
       vec.push_back(&v);
    });
}

size_t BlocksToCfa::getNumUsesInBlocks(const llvm::Instruction* inst) const
{
    return llvm::count_if(classof_range<llvm::Instruction>(inst->users()), [this](auto* i) {
        return mGenInfo.Blocks.count(i->getParent()) != 0;
    });
}

template<class Range>
static bool hasUsesInBlockRange(const llvm::Instruction* inst, Range& range)
{
    for (auto i : classof_range<llvm::Instruction>(inst->users())) {
        if (llvm::find(range, i->getParent()) != std::end(range)) {
            return true;
        }
    }

    return false;
}

template<class Range>
static bool checkUsesInBlockRange(const MemoryObjectDef* def, Range& range, bool searchInside)
{
    // Currently the memory object interface does not support querying the uses of a particular def.
    // What we do instead is we query all uses of the underlying abstract memory object, and check
    // whether their reaching definition is 'def'.
    for (MemoryObjectUse& use : def->getObject()->uses()) {
        if (use.getReachingDef() != def) {
            continue;
        }

        llvm::BasicBlock* bb = use.getInstruction()->getParent();
        if (
            (searchInside && std::find(std::begin(range), std::end(range), bb) != std::end(range))
                ||
            (!searchInside && std::find(std::begin(range), std::end(range), bb) == std::end(range))
        ) {
            return true;
        }
    }

    return false;
}

template<class Range>
static bool hasUsesInBlockRange(const MemoryObjectDef* def, Range& range)
{
    return checkUsesInBlockRange(def, range, true);
}

template<class Range>
static bool hasUsesOutsideOfBlockRange(const MemoryObjectDef* def, Range& range)
{
    return checkUsesInBlockRange(def, range, false);
}

static bool isErrorBlock(llvm::BasicBlock* bb)
{
    // In error blocks, the last instruction before a terminator should be the  'gazer.error_code' call.
    auto inst = bb->getTerminator()->getPrevNonDebugInstruction();
    if (auto call = llvm::dyn_cast_or_null<CallInst>(inst)) {
        Function* function = call->getCalledFunction();
        if (function != nullptr && function->getName() == CheckRegistry::ErrorFunctionName) {
            return true;
        }
    }

    return false;
}

/// If \p bb is part of a loop nested into the CFA represented by \p genInfo, returns this loop.
/// Otherwise, this function returns nullptr.
static llvm::Loop* getNestedLoopOf(GenerationContext& genCtx, CfaGenInfo& genInfo, const llvm::BasicBlock* bb)
{
    auto nested = genCtx.getLoopInfoFor(bb->getParent())->getLoopFor(bb);
    if (nested == nullptr) {
        // This block is not part of a loop.
        return nullptr;
    }

    if (auto loop = genInfo.getSourceLoop()) {
        // The parent procedure was obtained from a loop, see if 'nested'
        // is actually a nested loop of our source.
        assert(nested != loop);

        llvm::Loop* current = nested;
        while ((current = current->getParentLoop()) != nullptr) {
            if (current == loop) {
                // 'loop' is the parent of 'nested', return the nested loop
                return nested;
            }
        }

        return nullptr;
    }

    if (genInfo.isSourceFunction()) {
        assert(nested->getParentLoop() == nullptr && "Function are only allowed to enter into top-level loops");
        assert(nested->getHeader() == bb && "There is a jump into a loop block which is not its header. Perhaps the CFG is irreducible?");
        return nested->getParentLoop() == nullptr ? nested : nullptr;
    }

    llvm_unreachable("A CFA source should either be a function or a loop.");
}

static std::string getLoopName(const llvm::Loop* loop, unsigned& loopCount, llvm::StringRef prefix)
{
    const BasicBlock* header = loop->getHeader();
    assert(header != nullptr && "Loop without a loop header?");

    std::string name = prefix;
    name += '/';
    if (header->hasName()) {
        name += header->getName();
    } else {
        name += "__loop_" + std::to_string(loopCount);
        ++loopCount;
    }

    return name;
}

ModuleToCfa::ModuleToCfa(
    llvm::Module& llvmModule,
    LoopInfoFuncTy loops,
    GazerContext& context,
    MemoryModel& memoryModel,
    LLVMTypeTranslator& types,
    const SpecialFunctions& specialFunctions,
    const LLVMFrontendSettings& settings
) : mModule(llvmModule),
    mContext(context),
    mMemoryModel(memoryModel),
    mSettings(settings),
    mSystem(std::make_unique<AutomataSystem>(context)),
    mGenCtx(*mSystem, mMemoryModel, types, std::move(loops), specialFunctions, settings)
{
    if (mSettings.simplifyExpr) {
        mExprBuilder = CreateFoldingExprBuilder(mContext);
    } else {
        mExprBuilder = CreateExprBuilder(mContext);
    }
}

std::unique_ptr<AutomataSystem> ModuleToCfa::generate(CfaToLLVMTrace& cfaToLlvmTrace)
{
    // Create all automata and interfaces.
    this->createAutomata();

    // Encode all loops and functions
    for (auto& [source, genInfo] : mGenCtx.procedures()) {
        LLVM_DEBUG(llvm::dbgs() << "Encoding function CFA " << genInfo.Automaton->getName() << "\n");

        BlocksToCfa blocksToCfa(mGenCtx, genInfo, *mExprBuilder);

        // Do the actual encoding.
        blocksToCfa.encode();
    }

    // CFAs must be connected graphs. Remove unreachable components now.
    for (auto& cfa : *mSystem) {
        // We do not want to remove the exit location - if it is unreachable,
        // create a dummy 'false' edge from the entry location to the exit.
        if (cfa.getExit()->getNumIncoming() == 0) {
            cfa.createAssignTransition(cfa.getEntry(), cfa.getExit(), mExprBuilder->False());
        }
        cfa.removeUnreachableLocations();
    }

    // If there is a procedure called 'main', set it as the entry automaton.
    Cfa* main = mSystem->getAutomatonByName(mSettings.function);
    assert(main != nullptr && "The main automaton must exist!");
    mSystem->setMainAutomaton(main);

    cfaToLlvmTrace = std::move(mGenCtx.getTraceInfo());

    return std::move(mSystem);
}

void ModuleToCfa::createAutomata()
{
    // Create an automaton for each function definition and set the interfaces.
    for (llvm::Function& function : mModule.functions()) {
        if (function.isDeclaration()) {
            continue;
        }

        auto& memoryInstHandler = mMemoryModel.getMemoryInstructionHandler(function);

        Cfa* cfa = mSystem->createCfa(function.getName());
        LLVM_DEBUG(llvm::dbgs() << "Created CFA " << cfa->getName() << "\n");
        DenseSet<BasicBlock*> visitedBlocks;

        // Create a CFA for each loop nested in this function
        LoopInfo* loopInfo = mGenCtx.getLoopInfoFor(&function);

        unsigned loopCount = 0;
        auto loops = loopInfo->getLoopsInPreorder();

        for (Loop* loop : loops) {
            std::string name = getLoopName(loop, loopCount, cfa->getName());

            Cfa* nested = mSystem->createCfa(name);
            LLVM_DEBUG(llvm::dbgs() << "Created nested CFA " << nested->getName() << "\n");
            mGenCtx.createLoopCfaInfo(nested, loop);
        }

        for (auto li = loops.rbegin(), le = loops.rend(); li != le; ++li) {
            Loop* loop = *li;
            CfaGenInfo& loopGenInfo = mGenCtx.getLoopCfa(loop);
            Cfa* nested = loopGenInfo.Automaton;

            LLVM_DEBUG(llvm::dbgs() << "Translating loop " << loop->getName() << "\n");

            ArrayRef<BasicBlock*> loopBlocks = loop->getBlocks();
            std::vector<BasicBlock*> loopOnlyBlocks;
            llvm::copy_if(
                loopBlocks,
                std::back_inserter(loopOnlyBlocks),
                [&visitedBlocks] (auto b) { return visitedBlocks.count(b) == 0; }
            );

            // Declare loop variables.
            this->declareLoopVariables(loop, loopGenInfo, memoryInstHandler,
                loopBlocks, loopOnlyBlocks, visitedBlocks);

            // Create locations for the blocks
            for (BasicBlock* bb : loopOnlyBlocks) {
                LLVM_DEBUG(llvm::dbgs() << "[LoopOnly] " << bb->getName() << "\n");
                Location* entry = nested->createLocation();
                Location* exit = isErrorBlock(bb) ? nested->createErrorLocation() : nested->createLocation();

                loopGenInfo.addBlockToLocationsMapping(bb, entry, exit);
            }

            // Store which block was the exiting block inside the loop
            llvm::SmallVector<llvm::Loop::Edge, 4> exitEdges;
            loop->getExitEdges(exitEdges);

            if (exitEdges.size() != 1) {
                loopGenInfo.ExitVariable = nested->createLocal(LoopOutputSelectorName, IntType::Get(mContext));
                nested->addOutput(loopGenInfo.ExitVariable);
                for (size_t i = 0; i < exitEdges.size(); ++i) {
                    LLVM_DEBUG(llvm::dbgs() << " Registering exit edge " << exitEdges[i].first->getName() << " --> " << exitEdges[i].second->getName() << "\n");
                    loopGenInfo.ExitEdges[exitEdges[i]] = mExprBuilder->IntLit(i);
                }
            }

            visitedBlocks.insert(loop->getBlocks().begin(), loop->getBlocks().end());
        }

        // Now that all loops in this function have been dealt with, translate the function itself.
        CfaGenInfo& genInfo = mGenCtx.createFunctionCfaInfo(cfa, &function);
        VariableDeclExtensionPoint functionVarDecl(genInfo);

        // Add function input and output parameters
        for (llvm::Argument& argument : function.args()) {
            functionVarDecl.createInput(&argument, mGenCtx.getTypes().get(argument.getType()));
        }

        // Add return value if there is one.
        if (!function.getReturnType()->isVoidTy()) {
            auto retval = cfa->createLocal(
                FunctionReturnValueName, mGenCtx.getTypes().get(function.getReturnType())
            );
            genInfo.ReturnVariable = retval;
            cfa->addOutput(retval);
        }

        // At this point, the loops are already encoded, we only need to handle the blocks outside of the loops.
        std::vector<BasicBlock*> functionBlocks;
        std::for_each(function.begin(), function.end(), [&visitedBlocks, &functionBlocks] (auto& bb) {
            if (visitedBlocks.count(&bb) == 0) {
                functionBlocks.push_back(&bb);
            }
        });

        // Add memory object definitions as variables
        memoryInstHandler.declareFunctionVariables(functionVarDecl);

        // For the local variables, we only need to add the values not present in any of the loops.
        for (BasicBlock& bb : function) {
            LLVM_DEBUG(llvm::dbgs() << "Translating function-level block " << bb.getName() << "\n");
            for (Instruction& inst : bb) {
                LLVM_DEBUG(llvm::dbgs().indent(2) << "Instruction " << inst.getName() << "\n");
                if (auto loop = loopInfo->getLoopFor(&bb)) {
                    // If the variable is an output of a loop, add it here as a local variable
                    Variable* output = mGenCtx.getLoopCfa(loop).findOutput(&inst);
                    if (output == nullptr && !hasUsesInBlockRange(&inst, functionBlocks)) {
                        LLVM_DEBUG(llvm::dbgs().indent(4) << "Not adding (no uses in function) " << inst << "\n");
                        continue;
                    }
                }

                if (!inst.getType()->isVoidTy()) {
                    functionVarDecl.createLocal(&inst, mGenCtx.getTypes().get(inst.getType()));
                }
            }
        }

        for (BasicBlock* bb : functionBlocks) {
            Location* entry = cfa->createLocation();
            Location* exit = isErrorBlock(bb) ? cfa->createErrorLocation() : cfa->createLocation();
            genInfo.addBlockToLocationsMapping(bb, entry, exit);
        }
    }
}

void ModuleToCfa::declareLoopVariables(
    llvm::Loop* loop, CfaGenInfo& loopGenInfo,
    MemoryInstructionHandler& memoryInstHandler,
    llvm::ArrayRef<llvm::BasicBlock*> loopBlocks,
    llvm::ArrayRef<llvm::BasicBlock*> loopOnlyBlocks,
    llvm::DenseSet<llvm::BasicBlock*>& visitedBlocks)
{
    // Create the appropriate extension point.
    LoopVarDeclExtensionPoint loopVarDecl(loopGenInfo);

    // Ask the memory model to declare possible memory-related loop variables.
    memoryInstHandler.declareLoopProcedureVariables(loop, loopVarDecl);

    // Create loop variables inst-by-inst
    for (BasicBlock* bb : loopBlocks) {
        for (Instruction& inst : *bb) {
            Variable* variable = nullptr;

            LLVM_DEBUG(llvm::dbgs() << " Visiting instruction " << inst << "\n");
            if (inst.getOpcode() == Instruction::PHI && bb == loop->getHeader()) {
                // PHI nodes of the entry block should be inputs.
                variable = loopVarDecl.createPhiInput(&inst, mGenCtx.getTypes().get(inst.getType()));
                LLVM_DEBUG(llvm::dbgs() << "    Added PHI input variable " << *variable << "\n");
            } else {
                // Add operands which were defined in the caller as inputs
                for (auto oi = inst.op_begin(), oe = inst.op_end(); oi != oe; ++oi) {
                    llvm::Value* value = *oi;
                    if (isDefinedInCaller(value, loopBlocks) && !loopGenInfo.hasInput(value)) {
                        auto argVariable = loopVarDecl.createInput(
                            value, mGenCtx.getTypes().get(value->getType())
                        );
                        LLVM_DEBUG(llvm::dbgs() << "    Added input variable " << *argVariable << ", instruction " << inst << "\n");
                    }
                }

                // Do not create a variable if the instruction has no return type.
                if (inst.getType()->isVoidTy()) {
                    continue;
                }

                // If the instruction is defined in an already visited block, it is a local of
                // a subloop rather than this loop.
                if (visitedBlocks.count(bb) == 0 || hasUsesInBlockRange(&inst, loopOnlyBlocks)) {
                    variable = loopVarDecl.createLocal(&inst, mGenCtx.getTypes().get(inst.getType()));
                    LLVM_DEBUG(llvm::dbgs() << "    Added local variable " << *variable << "\n");
                }
            }

            // Check if the instruction has users outside of the loop region.
            // If so, their corresponding variables must be marked as outputs.
            for (auto user : inst.users()) {
                if (auto i = llvm::dyn_cast<Instruction>(user)) {
                    if (std::find(loopBlocks.begin(), loopBlocks.end(), i->getParent()) == loopBlocks.end()) {
                        loopVarDecl.createLoopOutput(&inst, variable);
                        break;
                    }
                }
            }
        }
    }
}

BlocksToCfa::BlocksToCfa(
    GenerationContext& generationContext,
    CfaGenInfo& genInfo,
    ExprBuilder& exprBuilder
) : InstToExpr(
        *genInfo.getEntryBlock()->getParent(),
        exprBuilder,
        generationContext.getTypes(),
        genInfo.getMemoryInstructionHandler(),
        generationContext.getSettings()
    ),
    mGenCtx(generationContext),
    mGenInfo(genInfo),
    mCfa(mGenInfo.Automaton),
    mEntryBlock(mGenInfo.getEntryBlock())
{
    assert(mGenInfo.Blocks.count(mEntryBlock) != 0 && "Entry block must be in the block map!");
}

void BlocksToCfa::encode()
{
    Location* first = mGenInfo.Blocks[mEntryBlock].first;

    // Create a transition between the initial location and the entry block.
    mCfa->createAssignTransition(mCfa->getEntry(), first, mExprBuilder.True());

    for (auto [bb, pair] : mGenInfo.Blocks) {
        Location* entry = pair.first;
        Location* exit = pair.second;

        std::vector<VariableAssignment> assignments;

        auto ep = this->createExtensionPoint(assignments, &entry, &exit);

        // Handle block-level memory annotations first
        mMemoryInstHandler.handleBlock(*bb, ep);

        // Translate instructions one-by-one
        for (auto it = bb->getFirstInsertionPt(), ie = bb->end(); it != ie; ++it) {
            const llvm::Instruction& inst = *it;

            if (auto call = llvm::dyn_cast<CallInst>(&inst)) {
                bool generateAssignmentAfter = this->handleCall(call, &entry, exit, assignments);
                if (!generateAssignmentAfter) {
                    continue;
                }
            } else if (auto store = llvm::dyn_cast<StoreInst>(&inst)) {
                mMemoryInstHandler.handleStore(*store, ep);
                continue;
            }

            if (inst.getType()->isVoidTy()) {
                continue;
            }

            Variable* variable = getVariable(&inst);
            ExprPtr expr;

            if (auto load = llvm::dyn_cast<LoadInst>(&inst)) {
                expr = mMemoryInstHandler.handleLoad(*load, ep);
            } else if (auto alloca = llvm::dyn_cast<AllocaInst>(&inst)) {
                expr = mMemoryInstHandler.handleAlloca(*alloca, ep);
            } else {
                expr = this->transform(inst, variable->getType());
            }

            if (!ep.tryToEliminate(&inst, variable, expr)) {
                ep.insertAssignment(variable, expr);
            }
        }

        mCfa->createAssignTransition(entry, exit, mExprBuilder.True(), assignments);

        // Handle the outgoing edges
        this->handleTerminator(bb, entry, exit);
    }

    // Do a clean-up, remove eliminated variables from the CFA.
    if (!mGenCtx.getSettings().isElimVarsOff()) {
        mCfa->removeLocalsIf([this](auto v) {
            return mEliminatedVarsSet.count(v) != 0;
        });
    }
}

bool BlocksToCfa::handleCall(const llvm::CallInst* call, Location** entry, Location* exit, std::vector<VariableAssignment>& previousAssignments)
{
    Function* callee = call->getCalledFunction();
    if (callee == nullptr) {
        // We do not support indirect calls yet.
        return false;
    }

    // Create an extension point to handle the call
    auto callerEP = this->createExtensionPoint(previousAssignments, entry, &exit);

    if (!callee->isDeclaration()) {
        CfaGenInfo& calledAutomatonInfo = mGenCtx.getFunctionCfa(callee);
        Cfa* calledCfa = calledAutomatonInfo.Automaton;
        assert(calledCfa != nullptr && "The callee automaton must exist in a function call!");

        // Split the current transition here and create a call.
        Location* callBegin = mCfa->createLocation();
        Location* callEnd = mCfa->createLocation();

        mCfa->createAssignTransition(*entry, callBegin, previousAssignments);
        previousAssignments.clear();

        llvm::SmallVector<VariableAssignment, 4> inputs;
        llvm::SmallVector<VariableAssignment, 4> outputs;
        std::vector<VariableAssignment> additionalAssignments;

        // Insert regular LLVM IR arguments.
        llvm::SmallVector<llvm::Argument*, 4> arguments;
        for (llvm::Argument& arg : callee->args()) {
            arguments.push_back(&arg);
        }

        for (size_t i = 0; i < call->getNumArgOperands(); ++i) {
            ExprPtr expr = this->operand(call->getArgOperand(i));
            Variable* input = calledAutomatonInfo.findInput(arguments[i]);
            assert(input != nullptr && "Function arguments should be present as procedure inputs!");

            inputs.emplace_back(input, expr);
        }

        // Insert arguments coming from the memory model.
        AutomatonInterfaceExtensionPoint calleeEP(calledAutomatonInfo);

        // FIXME: This const_cast is needed because the memory model interface must
        // take a mutable call site as ImmutableCallSite objects cannot be put into
        // a map properly. This should be removed as soon as something about that changes.
        mMemoryInstHandler.handleCall(const_cast<llvm::CallInst*>(call), callerEP, calleeEP, inputs, outputs);

        if (!callee->getReturnType()->isVoidTy()) {
            Variable* variable = getVariable(call);

            // Find the return variable of this function.
            Variable* retval = mGenCtx.getFunctionCfa(callee).ReturnVariable;
            assert(retval != nullptr && "A non-void function must have a return value!");

            outputs.emplace_back(variable, retval->getRefExpr());
        }

        mCfa->createCallTransition(callBegin, callEnd, calledCfa, inputs, outputs);

        // Continue the translation from the end location of the call.
        *entry = callEnd;

        // Do not generate an assignment for this call.
        return false;
    }

    if (callee->getName() == CheckRegistry::ErrorFunctionName) {
        assert(exit->isError() && "The target location of a 'gazer.error_code' call must be an error location!");

        llvm::Value* arg = call->getArgOperand(0);
        ExprPtr errorCodeExpr = operand(arg);
        mCfa->addErrorCode(exit, errorCodeExpr);
    } else {
        // Try to handle this value as a special function.
        mGenCtx.getSpecialFunctions().handle(call, callerEP);
    }

    return true;
}

void BlocksToCfa::handleTerminator(const llvm::BasicBlock* bb, Location* entry, Location* exit)
{
    auto terminator = bb->getTerminator();

    if (auto br = llvm::dyn_cast<BranchInst>(terminator)) {
        ExprPtr condition = br->isConditional() ? operand(br->getCondition()) : mExprBuilder.True();

        for (unsigned succIdx = 0; succIdx < br->getNumSuccessors(); ++succIdx) {
            BasicBlock* succ = br->getSuccessor(succIdx);
            ExprPtr succCondition = succIdx == 0 ? condition : mExprBuilder.Not(condition);

            handleSuccessor(succ, succCondition, bb, exit);
        }
    } else if (auto swi = llvm::dyn_cast<SwitchInst>(terminator)) {
        ExprPtr condition = operand(swi->getCondition());

        ExprPtr prevConds = mExprBuilder.True();
        for (auto ci = swi->case_begin(); ci != swi->case_end(); ++ci) {
            auto val = operand(ci->getCaseValue());
            auto succ = ci->getCaseSuccessor();

            ExprPtr succCondition = mExprBuilder.And(
                prevConds,
                mExprBuilder.Eq(condition, val)
            );
            prevConds = mExprBuilder.And(prevConds, mExprBuilder.NotEq(condition, val));

            handleSuccessor(succ, succCondition, bb, exit);
        }

        handleSuccessor(swi->getDefaultDest(), prevConds, bb, exit);
    } else if (auto ret = llvm::dyn_cast<ReturnInst>(terminator)) {
        if (ret->getReturnValue() == nullptr) {
            mCfa->createAssignTransition(exit, mCfa->getExit());
        } else {
            Variable* retval = mGenInfo.ReturnVariable;
            assert(retval != nullptr && "Functions with return values should have a RET_VAL output in the CFA!");
            mCfa->createAssignTransition(exit, mCfa->getExit(), {
                VariableAssignment{ retval, operand(ret->getReturnValue()) }
            });
        }
    } else if (terminator->getOpcode() == Instruction::Unreachable) {
        // We want automata to be connected and the exit location should be reachable.
        // As such, we insert a dummy transition from here to the exit.
        mCfa->createAssignTransition(exit, mCfa->getExit(), mExprBuilder.False());
    } else {
        LLVM_DEBUG(llvm::dbgs() << *terminator << "\n");
        llvm_unreachable("Unknown terminator instruction.");
    }
}

bool BlocksToCfa::tryToEliminate(ValueOrMemoryObject val, Variable* variable, const ExprPtr& expr)
{
    if (mGenCtx.getSettings().isElimVarsOff()) {
        return false;
    }

    // Never eliminate variables obtained from call instructions,
    // as they might be needed to obtain a counterexample.
    if (llvm::isa<llvm::CallInst>(val) || llvm::isa<memory::CallDef>(val)) {
        return false;
    }

    // Do not eliminate variables which are loop outputs, as these will be needed
    // for the output assignments.
    if (mGenInfo.LoopOutputs.count(val) != 0 || mGenInfo.Outputs.count(val) != 0) {
        return false;
    }

    // Do not eliminate undef's.
    if (expr->getKind() == Expr::Undef) {
        return false;
    }

    if (val.isValue() && llvm::isa<llvm::Instruction>(val.asValue())) {
        auto inst = llvm::cast<llvm::Instruction>(val.asValue());
        // On 'Normal' level, we do not want to inline expressions which have multiple uses
        // and have already inlined operands.

        bool hasInlinedOperands = llvm::any_of(inst->operands(), [this](const llvm::Use& op) {
            return llvm::isa<Instruction>(op) && mInlinedVars.count(llvm::cast<Instruction>(op)) != 0;
        });

        if (!mGenCtx.getSettings().isElimVarsAggressive()
            && getNumUsesInBlocks(inst) > 1
            && hasInlinedOperands
        ) {
            return false;
        }
    }

    mInlinedVars[val] = expr;
    mGenCtx.addExprValueIfTraceEnabled(mGenInfo.Automaton, val, expr);
    mEliminatedVarsSet.insert(variable);
    return true;
}

void BlocksToCfa::createExitTransition(const BasicBlock* source, const BasicBlock* target, Location* pred, const ExprPtr& succCondition)
{
    LLVM_DEBUG(llvm::dbgs() << "  Building exit transition for block " << target->getName() << "\n");

    // If the target is outside of our region, create a simple edge to the exit.
    std::vector<VariableAssignment> exitAssigns;
    if (mGenInfo.ExitVariable != nullptr) {
        // If there are multiple exits, create an assignment to indicate which one to take.
        ExprPtr exitVal = mGenInfo.ExitEdges[{source, target}];
        assert(exitVal != nullptr && "An exit block must be present in the exit blocks map!");

        exitAssigns.emplace_back(mGenInfo.ExitVariable, exitVal);
    }

    // Add the possible loop exit assignments
    for (auto& [_, assign] : mGenInfo.LoopOutputs) {
        exitAssigns.push_back(assign);
    }

    mCfa->createAssignTransition(pred, mCfa->getExit(), succCondition, exitAssigns);
}

ExprPtr BlocksToCfa::getExitCondition(const llvm::BasicBlock* source, const llvm::BasicBlock* target, Variable* exitSelector, CfaGenInfo& nestedInfo)
{
    if (nestedInfo.ExitVariable == nullptr) {
        return mExprBuilder.True();
    }

    ExprPtr exitVal = nestedInfo.ExitEdges[{source, target}];
    assert(exitVal != nullptr && "An exit block must be present in the exit blocks map!");

    return mExprBuilder.Eq(exitSelector->getRefExpr(), exitVal);
}

void BlocksToCfa::handleSuccessor(const BasicBlock* succ, const ExprPtr& succCondition, const BasicBlock* parent,
    Location* exit)
{
    LLVM_DEBUG(llvm::dbgs() << "Translating CFG edge " << parent->getName() << " " << succ->getName() << "\n");
    if (succ == mEntryBlock) {
        // If the target is the loop header (entry block), create a call to this same automaton.
        auto loc = mCfa->createLocation();
        mCfa->createAssignTransition(exit, loc, succCondition);

        // Add possible calls arguments.
        // Due to the SSA-formed LLVM IR, regular inputs are not modified by loop iterations.
        // For PHI inputs, we need to determine which parent block to use for expression translation.
        std::vector<VariableAssignment> loopArgs;
        for (auto& [valueOrMemObj, variable] : mGenInfo.PhiInputs) {
            if (valueOrMemObj.isValue()) {
                auto incoming = cast<PHINode>(valueOrMemObj.asValue())->getIncomingValueForBlock(parent);
                loopArgs.emplace_back(variable, operand(incoming));
            } else {
                auto incoming = cast<memory::PhiDef>(valueOrMemObj.asMemoryObjectDef())->getIncomingDefForBlock(parent);
                loopArgs.emplace_back(variable, operand(incoming));
            }
        }

        for (auto& [valueOrMemObj, variable] : mGenInfo.Inputs) {
            ExprPtr argExpr = operand(valueOrMemObj);
            loopArgs.emplace_back(variable, argExpr);
        }

        std::vector<VariableAssignment> outputArgs;
        insertOutputAssignments(mGenInfo, outputArgs);

        if (mGenInfo.ExitVariable != nullptr) {
            outputArgs.emplace_back(mGenInfo.ExitVariable, mGenInfo.ExitVariable->getRefExpr());
        }

        mCfa->createCallTransition(loc, mCfa->getExit(), mExprBuilder.True(), mCfa, loopArgs, outputArgs);
    } else if (mGenInfo.Blocks.count(succ) != 0) {
        // Else if the target is is inside the block region, just create a simple edge.
        Location* to = mGenInfo.Blocks[succ].first;

        std::vector<VariableAssignment> phiAssignments;
        insertPhiAssignments(parent, succ, phiAssignments);

        auto phiEp = this->createExtensionPoint(phiAssignments, &exit, &to);
        mMemoryInstHandler.handleBasicBlockEdge(*parent, *succ, phiEp);

        mCfa->createAssignTransition(exit, to, succCondition, phiAssignments);
    } else if (auto loop = getNestedLoopOf(mGenCtx, mGenInfo, succ)) {
        assert(loop->getHeader() == succ && "Target successor in a loop must be the loop header; maybe the CFG is irreducible?");
        this->createCallToLoop(loop, parent, succCondition, exit);
    } else {
        createExitTransition(parent, succ, exit, succCondition);
    }
}

void BlocksToCfa::createCallToLoop(llvm::Loop* loop, const llvm::BasicBlock* source, const ExprPtr& condition, Location* exit)
{
    LLVM_DEBUG(llvm::dbgs() << " Building call for loop " << loop->getName() << "\n");
    // If this is a nested loop, create a call to the corresponding automaton.
    CfaGenInfo& nestedLoopInfo = mGenCtx.getLoopCfa(loop);
    auto nestedCfa = nestedLoopInfo.Automaton;

    LLVM_DEBUG(nestedCfa->printDeclaration(llvm::dbgs()));

    std::vector<VariableAssignment> loopArgs;
    for (auto& [valueOrMemObj, variable] : nestedLoopInfo.PhiInputs) {
        LLVM_DEBUG(llvm::dbgs() << "  Translating loop PHI argument " << *variable << " " << valueOrMemObj << "\n");
        if (valueOrMemObj.isValue()) {
            auto incoming = cast<PHINode>(valueOrMemObj.asValue())->getIncomingValueForBlock(source);
            loopArgs.emplace_back(variable, operand(incoming));
        } else {
            auto incoming = cast<memory::PhiDef>(valueOrMemObj.asMemoryObjectDef())->getIncomingDefForBlock(source);
            loopArgs.emplace_back(variable, operand(incoming));
        }
    }

    for (auto& [valueOrMemObj, variable] : nestedLoopInfo.Inputs) {
        LLVM_DEBUG(llvm::dbgs() << "  Translating loop input argument " << *variable << " " << valueOrMemObj << "\n");
        ExprPtr argExpr = operand(valueOrMemObj);
        loopArgs.emplace_back(variable, argExpr);
    }

    std::vector<VariableAssignment> outputArgs;
    insertOutputAssignments(nestedLoopInfo, outputArgs);

    Variable* exitSelector = nullptr;
    if (nestedLoopInfo.ExitVariable != nullptr) {
        exitSelector = mCfa->createLocal(
            Twine(ModuleToCfa::LoopOutputSelectorName).concat(Twine(mCounter++)).str(),
            IntType::Get(mContext)
        );
        outputArgs.emplace_back(exitSelector, nestedLoopInfo.ExitVariable->getRefExpr());
    }

    auto loc = mCfa->createLocation();
    mCfa->createCallTransition(exit, loc, condition, nestedCfa, loopArgs, outputArgs);

    // Handle the possible exiting edges of the inner loop
    llvm::SmallVector<llvm::Loop::Edge, 4> exitEdges;
    loop->getExitEdges(exitEdges);

    for (const auto& [inBlock, exitBlock] : exitEdges) {
        LLVM_DEBUG(llvm::dbgs() << "  Handling exit edge " << inBlock->getName()
            << " to " << exitBlock->getName() << "\n");
        std::vector<VariableAssignment> phiAssignments;
        insertPhiAssignments(inBlock, exitBlock, phiAssignments);

        auto result = mGenInfo.Blocks.find(exitBlock);
        if (result != mGenInfo.Blocks.end()) {
            // The exit edge is within the block range of the current automaton,
            // simply create an assign transition to represent the exit jump.
            mCfa->createAssignTransition(
                loc, result->second.first,
                getExitCondition(inBlock, exitBlock, exitSelector, nestedLoopInfo),
                phiAssignments
            );
        } else {
            createExitTransition(inBlock, exitBlock, exit, condition);
        }
    }
}

void BlocksToCfa::insertOutputAssignments(CfaGenInfo& callee, std::vector<VariableAssignment>& outputArgs)
{
    // For the outputs, find the corresponding variables in the parent and create the assignments.
    for (auto& [value, nestedOutputVar] : callee.Outputs) {
        LLVM_DEBUG(
            llvm::dbgs() << "  Inserting output assignment for " << value.getName()
            << " variable " << *nestedOutputVar << "\n"
        );

        // It is either a local or input in parent.
        Variable* parentVar;

        // If the value is a loop output, we must use its "_out" variable.
        if (auto loopVar = mGenInfo.LoopOutputs.find(value); loopVar != mGenInfo.LoopOutputs.end()) {
            parentVar = loopVar->second.getVariable();
        } else {
            parentVar = mGenInfo.findVariable(value);
        }

        assert(parentVar != nullptr && "Nested output variable should be present in parent as an input or local!");
        outputArgs.emplace_back(parentVar, this->castResult(nestedOutputVar->getRefExpr(), parentVar->getType()));
    }
}

void BlocksToCfa::insertPhiAssignments(
    const BasicBlock* source,
    const BasicBlock* target,
    std::vector<VariableAssignment>& phiAssignments)
{
    // Handle regular LLVM PHI values.
    auto it = target->begin();
    while (llvm::isa<PHINode>(it)) {
        auto phi = llvm::cast<PHINode>(it);
        Value* incoming = phi->getIncomingValueForBlock(source);

        Variable* variable = getVariable(phi);
        ExprPtr expr = operand(incoming);

        phiAssignments.emplace_back(variable, expr);

        ++it;
    }
}

ExprPtr BlocksToCfa::lookupInlinedVariable(ValueOrMemoryObject value)
{
    return mInlinedVars.lookup(value);
}

Variable* BlocksToCfa::getVariable(ValueOrMemoryObject value)
{
    auto result = mGenInfo.findVariable(value);
    assert(result != nullptr && "Variables should be present in one of the variable maps!");

    return result;
}
