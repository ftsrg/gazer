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
            return false;
        }

        return true;
    }

    // TODO: Is this always correct?
    return false;
}

template<class AccessKind, class Range>
static void memoryAccessOfKind(Range&& range, llvm::SmallVectorImpl<AccessKind*>& vec)
{
    static_assert(std::is_base_of_v<MemoryAccess, AccessKind>, "AccessKind must be a subclass of MemoryAccess!");

    for (auto& access : range) {
        if (auto casted = llvm::dyn_cast<AccessKind>(&access)) {
            vec.push_back(casted);
        }
    }
}

size_t BlocksToCfa::getNumUsesInBlocks(const llvm::Instruction* inst) const
{
    size_t cnt = 0;
    for (auto user : inst->users()) {
        if (auto i = llvm::dyn_cast<Instruction>(user)) {
            if (mGenInfo.Blocks.count(i->getParent()) != 0 ) {
                cnt += 1;
            }
        }
    }

    return cnt;
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
static bool checkUsesInBLockRange(const MemoryObjectDef* def, Range&& range, bool searchInside)
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
static bool hasUsesInBlockRange(const MemoryObjectDef* def, Range&& range)
{
    return checkUsesInBLockRange(def, range, true);
}

template<class Range>
static bool hasUsesOutsideOfBlockRange(const MemoryObjectDef* def, Range&& range)
{
    return checkUsesInBLockRange(def, range, false);
}

static bool isErrorBlock(llvm::BasicBlock* bb)
{
    if (!llvm::isa<UnreachableInst>(bb->getTerminator())) {
        // Error blocks must be terminated by an unreachable
        return false;
    }

    // In error blocks, the last instruction before a terminator should be the 'gazer.error_code' call.
    auto inst = bb->getTerminator()->getPrevNonDebugInstruction();

    if (auto call = llvm::dyn_cast<CallInst>(inst)) {
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
        while ((current = current->getParentLoop())) {
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

static gazer::Type& getExitSelectorType(IntRepresentation ints, GazerContext& context)
{         
    switch (ints) {
        case IntRepresentation::BitVectors:
            return BvType::Get(context, 8);
        case IntRepresentation::Integers:
            return IntType::Get(context);
    }
    
    llvm_unreachable("Invalid int representation strategy!");
}

static std::string getLoopName(llvm::Loop* loop, unsigned& loopCount, llvm::StringRef prefix)
{
    BasicBlock* header = loop->getHeader();
    assert(header != nullptr && "Loop without a loop header?");

    std::string name = prefix;
    name += '/';
    if (header->hasName()) {
        name += header->getName();
    } else {
        name += "__loop_" + std::to_string(loopCount++);
    }

    return name;
}

ModuleToCfa::ModuleToCfa(
    llvm::Module& module,
    GenerationContext::LoopInfoMapTy& loops,
    GazerContext& context,
    MemoryModel& memoryModel,
    LLVMFrontendSettings settings
) : mModule(module),
    mContext(context),
    mMemoryModel(memoryModel),
    mSettings(settings),
    mSystem(new AutomataSystem(context)),
    mGenCtx(*mSystem, mMemoryModel, loops, settings)
{
    if (mSettings.simplifyExpr) {
        mExprBuilder = CreateFoldingExprBuilder(mContext);
    } else {
        mExprBuilder = CreateExprBuilder(mContext);
    }
}

std::unique_ptr<AutomataSystem> ModuleToCfa::generate(
    llvm::DenseMap<llvm::Value*, Variable*>& variables,
    CfaToLLVMTrace& cfaToLlvmTrace
) {
    // Create all automata and interfaces.
    this->createAutomata();

    // Encode all loops and functions
    for (auto& [source, genInfo] : mGenCtx.procedures()) {
        LLVM_DEBUG(llvm::dbgs() << "Encoding function CFA " << genInfo.Automaton->getName() << "\n");

        BlocksToCfa blocksToCfa(
            mGenCtx,
            genInfo,
            *mExprBuilder
        );

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
    Cfa* main = mSystem->getAutomatonByName("main");
    if (main != nullptr) {
        mSystem->setMainAutomaton(main);
    } else {
        llvm::errs() << "Warning: could not find a main automaton.\n";
    }

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

        memory::MemorySSA& memorySSA = *mMemoryModel.getFunctionMemorySSA(function);
        LLVM_DEBUG(memorySSA.print(llvm::dbgs()));

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

            ArrayRef<BasicBlock*> loopBlocks = loop->getBlocks();

            std::vector<BasicBlock*> loopOnlyBlocks;
            std::copy_if(
                loopBlocks.begin(), loopBlocks.end(),
                std::back_inserter(loopOnlyBlocks),
                [&visitedBlocks] (BasicBlock* b) { return visitedBlocks.count(b) == 0; }
            );

            // Declare loop variables.
            VariableDeclExtensionPoint loopVarDecl(loopGenInfo);
            for (BasicBlock* bb : loopBlocks) {
                // Check for possible memory object PHI's
                for (MemoryObjectDef& def : memorySSA.definitionAnnotationsFor(bb)) {
                    Variable* memVar;
                    if (bb == loop->getHeader() && def.getKind() == MemoryObjectDef::PHI) {
                        memVar = loopVarDecl.createPhiInput(
                            &def,
                            def.getObject()->getObjectType()
                        );
                    } else {
                        memVar = loopVarDecl.createLocal(
                            &def,
                            def.getObject()->getObjectType()
                        );
                    }

                    if (hasUsesOutsideOfBlockRange(&def, loopBlocks)) {
                        std::string name = def.getName() + "_out";
                        auto copyOfVar = nested->createLocal(name, memVar->getType());

                        LLVM_DEBUG(llvm::dbgs() << "Added loop output variable " <<
                             *copyOfVar << " for " << def << "size: " << loopGenInfo.Outputs.size() << "\n");

                        loopVarDecl.markOutput(&def, copyOfVar);
                        loopGenInfo.LoopOutputs[&def] = VariableAssignment{
                            copyOfVar, memVar->getRefExpr()
                        };
                    }
                }

                for (Instruction& inst : *bb) {
                    Variable* variable = nullptr;

                    // First, check the memory SSA annotations for this instruction.
                    for (MemoryObjectUse& use : memorySSA.useAnnotationsFor(&inst)) {
                        // If the use's reaching definition is outside of the loop, we add it as an input.
                        llvm::BasicBlock* defBlock = use.getReachingDef()->getParentBlock();
                        if (std::find(loopBlocks.begin(), loopBlocks.end(), defBlock) == loopBlocks.end()) {
                            loopVarDecl.createInput(
                                use.getReachingDef(),
                                use.getObject()->getObjectType()
                            );
                        }
                    }


                    // All definitions inside the loop will be locals.
                    for (MemoryObjectDef& def : memorySSA.definitionAnnotationsFor(&inst)) {
                        Variable* memVar = loopVarDecl.createLocal(
                            &def, def.getObject()->getObjectType()
                        );

                        // If the definition has uses outside of the loop, it should also be marked as output.
                        for (MemoryObjectUse& use : def.getObject()->uses()) {
                            if (use.getReachingDef() != &def) {
                                continue;
                            }

                            llvm::BasicBlock* useBlock = use.getInstruction()->getParent();
                            if (std::find(loopBlocks.begin(), loopBlocks.end(), useBlock) == loopBlocks.end()) {
                                std::string name = def.getName() + "_out";
                                auto copyOfVar = nested->createLocal(name, memVar->getType());

                                LLVM_DEBUG(llvm::dbgs() << "Added loop output variable " <<
                                     *copyOfVar << " for " << def << "size: " << loopGenInfo.Outputs.size() << "\n");

                                loopVarDecl.markOutput(&def, copyOfVar);
                                loopGenInfo.LoopOutputs[&def] = VariableAssignment{
                                    copyOfVar, memVar->getRefExpr()
                                };

                                break;
                            }
                        }
                    }

                    if (inst.getOpcode() == Instruction::PHI && bb == loop->getHeader()) {
                        // PHI nodes of the entry block should be inputs.
                        variable = loopVarDecl.createPhiInput(&inst, mMemoryModel.translateType(inst.getType()));
                        LLVM_DEBUG(llvm::dbgs() << "  Added PHI input variable " << *variable << "\n");
                    } else {
                        // Add operands which were defined in the caller as inputs
                        for (auto oi = inst.op_begin(), oe = inst.op_end(); oi != oe; ++oi) {
                            llvm::Value* value = *oi;
                            if (isDefinedInCaller(value, loopBlocks) && !loopGenInfo.hasInput(value)) {
                                auto argVariable = loopVarDecl.createInput(
                                    value, mMemoryModel.translateType(value->getType())
                                );
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
                            variable = loopVarDecl.createLocal(&inst, mMemoryModel.translateType(inst.getType()));
                            LLVM_DEBUG(llvm::dbgs() << "  Added local variable " << *variable << "\n");
                        }
                    }

                    // Check if the instruction has users outside of the loop region.
                    // If so, their corresponding variables must be marked as outputs.
                    for (auto user : inst.users()) {
                        if (auto i = llvm::dyn_cast<Instruction>(user)) {
                            if (std::find(loopBlocks.begin(), loopBlocks.end(), i->getParent()) == loopBlocks.end()) {
                                std::string name = inst.getName().str() + "_out";
                                auto copyOfVar = nested->createLocal(name, variable->getType());

                                loopVarDecl.markOutput(&inst, copyOfVar);
                                loopGenInfo.LoopOutputs[&inst] = VariableAssignment{ copyOfVar, variable->getRefExpr() };

                                LLVM_DEBUG(llvm::dbgs() << "  Added output variable " << *copyOfVar << "\n");
                                break;
                            }
                        }
                    }
                }
            }

            // Create locations for the blocks
            for (BasicBlock* bb : loopOnlyBlocks) {
                Location* entry = nested->createLocation();
                Location* exit = isErrorBlock(bb) ? nested->createErrorLocation() : nested->createLocation();

                loopGenInfo.addBlockToLocationsMapping(bb, entry, exit);
            }

            // If the loop has multiple exits, add a selector output to disambiguate between these.
            llvm::SmallVector<llvm::BasicBlock*, 4> exitBlocks;
            loop->getUniqueExitBlocks(exitBlocks);
            if (exitBlocks.size() != 1) {
                Type& selectorTy = getExitSelectorType(mSettings.ints, mContext);
                loopGenInfo.ExitVariable = nested->createLocal(LoopOutputSelectorName, selectorTy);
                nested->addOutput(loopGenInfo.ExitVariable);
                for (size_t i = 0; i < exitBlocks.size(); ++i) {
                    loopGenInfo.ExitBlocks[exitBlocks[i]] = selectorTy.isBvType() 
                        ? boost::static_pointer_cast<LiteralExpr>(mExprBuilder->BvLit(i, 8))
                        : mExprBuilder->IntLit(i);
                }
            }

            visitedBlocks.insert(loop->getBlocks().begin(), loop->getBlocks().end());
        }

        // Now that all loops in this function have been dealt with, translate the function itself.
        CfaGenInfo& genInfo = mGenCtx.createFunctionCfaInfo(cfa, &function);
        VariableDeclExtensionPoint functionVarDecl(genInfo);

        // Add function input and output parameters
        for (llvm::Argument& argument : function.args()) {
            functionVarDecl.createInput(&argument, mMemoryModel.translateType(argument.getType()));
        }

        // Add return value if there is one.
        if (!function.getReturnType()->isVoidTy()) {
            auto retval = cfa->createLocal(
                FunctionReturnValueName, mMemoryModel.translateType(function.getReturnType())
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
        for (MemoryObject& object : memorySSA.objects()) {
            for (MemoryObjectDef& def : object.defs()) {
                if (auto liveOnEntry = llvm::dyn_cast<memory::LiveOnEntryDef>(&def)) {
                    // TODO: Remove hard-coded main
                    if (function.getName() == "main") {
                        functionVarDecl.createLocal(&def, object.getObjectType(), "_mem");
                    } else {
                        functionVarDecl.createInput(&def, object.getObjectType(), "_mem");
                    }
                } else {
                    functionVarDecl.createLocal(&def, object.getObjectType(), "_mem");
                }
            }

            llvm::SmallVector<memory::RetUse*, 1> retUses;
            memoryAccessOfKind(object.uses(), retUses);

            assert(retUses.size() == 0 || retUses.size() == 1);
            if (!retUses.empty()) {
                Variable* output = genInfo.findVariable(retUses[0]->getReachingDef());
                functionVarDecl.markOutput(retUses[0]->getReachingDef(), output);
            }
        }

        // For the local variables, we only need to add the values not present in any of the loops.
        for (BasicBlock& bb : function) {
            for (Instruction& inst : bb) {
                if (auto loop = loopInfo->getLoopFor(&bb)) {
                    // If the variable is an output of a loop, add it here as a local variable
                    Variable* output = mGenCtx.getLoopCfa(loop).findOutput(&inst);
                    if (output == nullptr && !hasUsesInBlockRange(&inst, functionBlocks)) {
                        LLVM_DEBUG(llvm::dbgs() << "Skipped " << inst << "\n");
                        continue;
                    }
                }

                if (!inst.getType()->isVoidTy()) {
                    functionVarDecl.createLocal(&inst, mMemoryModel.translateType(inst.getType()));
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

BlocksToCfa::BlocksToCfa(
    GenerationContext& generationContext,
    CfaGenInfo& genInfo,
    ExprBuilder& exprBuilder
) : InstToExpr(
        *genInfo.getEntryBlock()->getParent(),
        exprBuilder,
        generationContext.getMemoryModel(),
        generationContext.getSettings()
    ),
    mGenInfo(genInfo),
    mGenCtx(generationContext),
    mCfa(mGenInfo.Automaton),
    mEntryBlock(mGenInfo.getEntryBlock())
{
    assert(mGenInfo.Blocks.count(mEntryBlock) != 0 && "Entry block must be in the block map!");
    mMemorySSA = mMemoryModel.getFunctionMemorySSA(mFunction);
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

        auto ep = this->createExtensionPoint(assignments);

        // Handle block-level memory annotations first
        mMemoryModel.handleBlock(*bb, ep);

        // Translate instructions one-by-one
        for (auto it = bb->getFirstInsertionPt(), ie = bb->end(); it != ie; ++it) {
            const llvm::Instruction& inst = *it;

            if (auto call = llvm::dyn_cast<CallInst>(&inst)) {
                bool generateAssignmentAfter = this->handleCall(call, &entry, exit, assignments);
                if (!generateAssignmentAfter) {
                    continue;
                }
            } else if (auto store = llvm::dyn_cast<StoreInst>(&inst)) {
                auto storeValue = this->operand(store->getValueOperand());
                auto ptr = this->operand(store->getPointerOperand());

                mMemoryModel.handleStore(*store, ptr, storeValue, ep);
                continue;
            }

            if (inst.getType()->isVoidTy()) {
                continue;
            }

            Variable* variable = getVariable(&inst);
            ExprPtr expr;

            if (auto load = llvm::dyn_cast<LoadInst>(&inst)) {
                auto ptr = this->operand(load->getPointerOperand());
                expr = mMemoryModel.handleLoad(*load, ptr, ep);
            } else if (auto alloca = llvm::dyn_cast<AllocaInst>(&inst)) {
                expr = mMemoryModel.handleAlloca(*alloca, ep);
            } else {
                expr = this->transform(inst);
            }

            if (!tryToEliminate(&inst, variable, expr)) {
                assignments.emplace_back(variable, expr);
            }
        }

        mCfa->createAssignTransition(entry, exit, mExprBuilder.True(), assignments);

        // Handle the outgoing edges
        this->handleTerminator(bb, entry, exit);
    }

    // Do a clean-up, remove eliminated variables from the CFA.
    if (!mGenCtx.getSettings().isElimVarsOff()) {
        mCfa->removeLocalsIf([this](Variable* v) {
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
    } else if (!callee->isDeclaration()) {
        CfaGenInfo& calledAutomatonInfo = mGenCtx.getFunctionCfa(callee);
        Cfa* calledCfa = calledAutomatonInfo.Automaton;

        assert(calledCfa != nullptr && "The callee automaton must exist in a function call!");
        
        // Split the current transition here and create a call.
        Location* callBegin = mCfa->createLocation();
        Location* callEnd = mCfa->createLocation();

        mCfa->createAssignTransition(*entry, callBegin, previousAssignments);
        previousAssignments.clear();

        std::vector<VariableAssignment> inputs;
        std::vector<VariableAssignment> outputs;
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
        auto callerEP = this->createExtensionPoint(previousAssignments);
        AutomatonInterfaceExtensionPoint calleeEP(calledAutomatonInfo);

        mMemoryModel.handleCall(call, callerEP, calleeEP, inputs, outputs, additionalAssignments);

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
    } else if (callee->getName() == CheckRegistry::ErrorFunctionName) {
        assert(exit->isError() && "The target location of a 'gazer.error_code' call must be an error location!");
        llvm::Value* arg = call->getArgOperand(0);

        ExprPtr errorCodeExpr = operand(arg);

        mCfa->addErrorCode(exit, errorCodeExpr);
    } else if (callee->getName() == "llvm.assume" || callee->getName() == "verifier.assume") {
        // Assumptions will split the current transition and insert a new assign transition,
        // with the guard being the assumption.
        llvm::Value* arg = call->getArgOperand(0);
        ExprPtr assumeExpr = operand(arg);

        // Create the new locations
        Location* assumeBegin = mCfa->createLocation();
        Location* assumeEnd = mCfa->createLocation();

        mCfa->createAssignTransition(*entry, assumeBegin, previousAssignments);
        previousAssignments.clear();

        mCfa->createAssignTransition(assumeBegin, assumeEnd, /*guard=*/assumeExpr);
        *entry = assumeEnd;
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

bool BlocksToCfa::tryToEliminate(ValueOrMemoryObject val, Variable* variable, ExprPtr expr)
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
        const llvm::Instruction* inst = llvm::cast<llvm::Instruction>(val.asValue());
        // On 'Normal' level, we do not want to inline expressions which have multiple uses
        // and have already inlined operands.

        bool hasInlinedOperands = std::any_of(inst->op_begin(), inst->op_end(), [this](const llvm::Use& op) {
            const llvm::Value* v = &*op;
            return llvm::isa<Instruction>(v) && mInlinedVars.count(llvm::cast<Instruction>(v)) != 0;
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

void BlocksToCfa::createExitTransition(const BasicBlock* target, Location* pred, const ExprPtr& succCondition)
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

ExprPtr BlocksToCfa::getExitCondition(const llvm::BasicBlock* target, Variable* exitSelector, CfaGenInfo& nestedInfo)
{
    if (nestedInfo.ExitVariable == nullptr) {
        return mExprBuilder.True();
    }

    ExprPtr exitVal = nestedInfo.ExitBlocks[target];
    assert(exitVal != nullptr && "An exit block must be present in the exit blocks map!");

    return mExprBuilder.Eq(exitSelector->getRefExpr(), exitVal);
}

void BlocksToCfa::handleSuccessor(const BasicBlock* succ, const ExprPtr& succCondition, const BasicBlock* parent,
    Location* exit)
{
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

        mCfa->createAssignTransition(exit, to, succCondition, phiAssignments);
    } else if (auto loop = getNestedLoopOf(mGenCtx, mGenInfo, succ)) {
        // If this is a nested loop, create a call to the corresponding automaton.
        CfaGenInfo& nestedLoopInfo = mGenCtx.getLoopCfa(loop);
        auto nestedCfa = nestedLoopInfo.Automaton;

        std::vector<VariableAssignment> loopArgs;
        for (auto& [valueOrMemObj, variable] : nestedLoopInfo.PhiInputs) {
            if (valueOrMemObj.isValue()) {
                auto incoming = cast<PHINode>(valueOrMemObj.asValue())->getIncomingValueForBlock(parent);
                loopArgs.emplace_back(variable, operand(incoming));
            } else {
                auto incoming = cast<memory::PhiDef>(valueOrMemObj.asMemoryObjectDef())->getIncomingDefForBlock(parent);
                loopArgs.emplace_back(variable, operand(incoming));
            }
        }

        for (auto& [valueOrMemObj, variable] : nestedLoopInfo.Inputs) {
            ExprPtr argExpr = operand(valueOrMemObj);
            loopArgs.emplace_back(variable, argExpr);
        }

        std::vector<VariableAssignment> outputArgs;
        insertOutputAssignments(nestedLoopInfo, outputArgs);

        Variable* exitSelector = nullptr;
        if (nestedLoopInfo.ExitVariable != nullptr) {
            Type& selectorTy = getExitSelectorType(mSettings.ints, mContext);

            exitSelector = mCfa->createLocal(
                Twine(ModuleToCfa::LoopOutputSelectorName).concat(Twine(mCounter++)).str(),
                selectorTy
            );
            outputArgs.emplace_back(exitSelector, nestedLoopInfo.ExitVariable->getRefExpr());
        }

        auto loc = mCfa->createLocation();
        mCfa->createCallTransition(exit, loc, succCondition, nestedCfa, loopArgs, outputArgs);

        llvm::SmallVector<llvm::Loop::Edge, 4> exitEdges;
        loop->getExitEdges(exitEdges);

        for (llvm::Loop::Edge& exitEdge : exitEdges) {
            const llvm::BasicBlock* inBlock = exitEdge.first;
            const llvm::BasicBlock* exitBlock = exitEdge.second;

            std::vector<VariableAssignment> phiAssignments;
            insertPhiAssignments(inBlock, exitBlock, phiAssignments);

            auto result = mGenInfo.Blocks.find(exitBlock);
            if (result != mGenInfo.Blocks.end()) {
                mCfa->createAssignTransition(
                    loc, result->second.first,
                    getExitCondition(exitBlock, exitSelector, nestedLoopInfo),
                    phiAssignments
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
    for (auto& [value, nestedOutputVar] : callee.Outputs) {
        LLVM_DEBUG(
            llvm::dbgs() << "  Inserting output assignment for " << value.getName()
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
        outputArgs.emplace_back(parentVar, this->castResult(nestedOutputVar->getRefExpr(), parentVar->getType()));
    }
}

void BlocksToCfa::insertPhiAssignments(
    const BasicBlock* source,
    const BasicBlock* target,
    std::vector<VariableAssignment>& phiAssignments)
{
    // Start with possible memory object PHI's.
    for (auto& def : mMemorySSA->definitionAnnotationsFor(target)) {
        if (auto phi = llvm::dyn_cast<memory::PhiDef>(&def)) {
            MemoryObjectDef* incoming = phi->getIncomingDefForBlock(source);
            Variable* variable = getVariable(phi);

            ExprPtr expr = operand(incoming);
            phiAssignments.emplace_back(variable, expr);
        }
    }

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
