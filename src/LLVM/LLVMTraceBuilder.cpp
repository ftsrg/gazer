#include "gazer/LLVM/LLVMTraceBuilder.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/Core/Expr/ExprEvaluator.h"
#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/raw_ostream.h>

using namespace gazer;
using namespace llvm;

static void updateCurrentValuation(Valuation& val, const std::vector<VariableAssignment>& action)
{
    for (auto& assign : action) {
        assert(assign.getValue()->getKind() == Expr::Literal);
        val[assign.getVariable()] = boost::static_pointer_cast<LiteralExpr>(assign.getValue());
    }
}

std::unique_ptr<Trace> LLVMTraceBuilder::build(
    std::vector<Location*>& states,
    std::vector<std::vector<VariableAssignment>>& actions)
{
    assert(states.size() == actions.size() + 1);
    assert(states.front() == states.front()->getAutomaton()->getEntry());

    std::vector<std::unique_ptr<TraceEvent>> events;

    Valuation currentVals;

    auto shouldProcessEntry = [](CfaToLLVMTrace::BlockToLocationInfo info) -> bool {
        return info.block != nullptr && info.kind == CfaToLLVMTrace::Location_Entry;
    };

    auto stateIt  = states.begin();
    auto stateEnd = states.end();

    auto actionIt = actions.begin();
    auto actionEnd = actions.end();

    while (stateIt != stateEnd) {
        Location* loc = *stateIt;
        auto entry = mCfaToLlvmTrace.getBlockFromLocation(loc);

        // We might have some 'helper' locations, which do not correspond to any
        // basic blocks. We will have to skip these.
        while (!shouldProcessEntry(entry) && actionIt != actionEnd) {
            entry = mCfaToLlvmTrace.getBlockFromLocation(*stateIt);
            updateCurrentValuation(currentVals, *actionIt);
            ++stateIt;
            ++actionIt;
        }

        if (actionIt == actionEnd) {
            // We have reached the last action - nothing else to do.
            return std::make_unique<Trace>(std::move(events));
        }
        
        loc = *stateIt;
        BasicBlock* bb = entry.block;

        for (llvm::Instruction& inst : *bb) {
            auto call = dyn_cast<CallInst>(&inst);
            if (!call) {
                continue;
            }

            llvm::Function* callee = call->getCalledFunction();
            if (auto dvi = llvm::dyn_cast<llvm::DbgValueInst>(&inst)) {
                if (dvi->getValue() && dvi->getVariable()) {
                    llvm::Value* value = dvi->getValue();
                    llvm::DILocalVariable* diVar = dvi->getVariable();

                    LocationInfo location = { 0, 0 };
                    if (auto valInst = llvm::dyn_cast<llvm::Instruction>(value)) {
                        llvm::DebugLoc debugLoc = nullptr;
                        if (valInst->getDebugLoc()) {
                            debugLoc = valInst->getDebugLoc();
                        } else if (dvi->getDebugLoc()) {
                            debugLoc = dvi->getDebugLoc();
                        }

                        if (debugLoc) {
                            location = { debugLoc->getLine(), debugLoc->getColumn() };
                        }
                    }

                    auto lit = this->getLiteralFromValue(loc->getAutomaton(), value, currentVals);
                    if (lit == nullptr) {
                        // XXX: Perhaps we should just emit an error here.
                        continue;
                    }

                    events.push_back(std::make_unique<AssignTraceEvent>(
                        diVar->getName(),
                        lit,
                        location
                    ));
                }
            } else if (callee->getName() == GazerIntrinsic::InlinedGlobalWriteName) {
                auto mdValue = cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata();
                auto value = cast<ValueAsMetadata>(mdValue)->getValue();

                auto mdGlobal = dyn_cast<DIGlobalVariable>(
                    cast<MetadataAsValue>(call->getArgOperand(1))->getMetadata()
                );

                auto lit = this->getLiteralFromValue(loc->getAutomaton(), value, currentVals);

                LocationInfo location = { 0, 0 };
                if (auto& debugLoc = call->getDebugLoc()) {
                    location = { debugLoc->getLine(), debugLoc->getColumn() };
                }

                events.push_back(std::make_unique<AssignTraceEvent>(
                    mdGlobal->getName(),
                    lit,
                    location
                ));
            } else if (callee->getName() == GazerIntrinsic::FunctionEntryName) {
                auto diSP = dyn_cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                events.push_back(std::make_unique<FunctionEntryEvent>(
                    diSP->getName()
                ));
            } else if (callee->getName() == GazerIntrinsic::FunctionReturnVoidName) {
                auto diSP = dyn_cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                events.push_back(std::make_unique<FunctionReturnEvent>(
                    diSP->getName(),
                    nullptr
                ));
            } else if (callee->getName() == GazerIntrinsic::FunctionCallReturnedName) {
                auto diSP = dyn_cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                events.push_back(std::make_unique<FunctionEntryEvent>(
                    diSP->getName()
                ));
            } else if (callee->isDeclaration() && !call->getType()->isVoidTy()) {
                // This is a function call to a nondetermistic function.
                auto variable = mCfaToLlvmTrace.getVariableForValue(loc->getAutomaton(), call);
                assert(variable != nullptr && "Call results should be present as variables!");

                ExprRef<AtomicExpr> expr = getLiteralFromValue(loc->getAutomaton(), call, currentVals);

                if (expr == nullptr) {
                    // For variables which are assigned but never read,
                    // it is possible to be not present in the model.
                    // For these variables, we are going insert an UndefExpr.
                    expr = UndefExpr::Get(variable->getType());
                }

                LocationInfo location = {0, 0};
                if (call->getDebugLoc()) {
                    location = {
                        call->getDebugLoc()->getLine(),
                        call->getDebugLoc()->getColumn()
                    };
                }

                events.push_back(std::make_unique<FunctionCallEvent>(
                    callee->getName(),
                    expr,
                    std::vector<ExprRef<AtomicExpr>>(),
                    location
                ));
            }
        }

        updateCurrentValuation(currentVals, *actionIt);
        ++stateIt;
        ++actionIt;
    }

    return std::make_unique<Trace>(std::move(events));
}

ExprRef<AtomicExpr> LLVMTraceBuilder::getLiteralFromValue(Cfa* cfa, const llvm::Value* value, Valuation& model)
{
    if (auto cd = dyn_cast<ConstantData>(value)) {
        return LiteralFromLLVMConst(mContext, cd);
    }

    auto expr = mCfaToLlvmTrace.getExpressionForValue(cfa, value);
    
    if (expr != nullptr) {
        return model.eval(expr);
    }

    return nullptr;
}

#if 0
std::vector<std::unique_ptr<TraceEvent>> LLVMTraceBuilderImpl::buildEventsFromBlocks(
    Valuation& model, const std::vector<llvm::BasicBlock*> traceBlocks
) {
    std::vector<std::unique_ptr<TraceEvent>> assigns;
    
    for (BasicBlock* bb : traceBlocks) {
        for (Instruction& instr : *bb) {
            auto call = llvm::dyn_cast<llvm::CallInst>(&instr);
            if (!call) {
                continue;
            }

            llvm::Function* callee = call->getCalledFunction();
            if (auto dvi = llvm::dyn_cast<llvm::DbgValueInst>(&instr)) {
                if (dvi->getValue() && dvi->getVariable()) {
                    llvm::Value* value = dvi->getValue();
                    llvm::DILocalVariable* diVar = dvi->getVariable();

                    LocationInfo location = { 0, 0 };
                    if (auto valInst = llvm::dyn_cast<llvm::Instruction>(value)) {
                        llvm::DebugLoc debugLoc = nullptr;
                        if (valInst->getDebugLoc()) {
                            debugLoc = valInst->getDebugLoc();
                        } else if (dvi->getDebugLoc()) {
                            debugLoc = dvi->getDebugLoc();
                        }

                        if (debugLoc) {
                            location = { debugLoc->getLine(), debugLoc->getColumn() };
                        }
                    }

                    auto lit = this->getLiteralFromValue(value, model);
                    if (lit == nullptr) {
                        // XXX: Perhaps we should just emit an error here.
                        continue;
                    }

                    assigns.push_back(std::make_unique<AssignTraceEvent>(
                        diVar->getName(),
                        lit,
                        location
                    ));
                }
            } else if (callee->getName() == GazerIntrinsic::InlinedGlobalWriteName) {
                auto mdValue = cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata();
                auto value = cast<ValueAsMetadata>(mdValue)->getValue();

                auto mdGlobal = dyn_cast<DIGlobalVariable>(
                    cast<MetadataAsValue>(call->getArgOperand(1))->getMetadata()
                );

                ExprRef<LiteralExpr> expr = nullptr;
                if (auto ci = dyn_cast<ConstantInt>(value)) {
                    expr = BvLiteralExpr::Get(BvType::Get(mContext, ci->getBitWidth()), ci->getValue());
                } else {
                    auto result = mValueMap.find(value);
                    if (result != mValueMap.end()) {
                        Variable* variable = result->second;
                        auto exprResult = model.find(variable);

                        if (exprResult != model.end()) {
                            expr = exprResult->second;
                        }
                    }
                }

                LocationInfo location = { 0, 0 };
                if (auto& debugLoc = call->getDebugLoc()) {
                    location = { debugLoc->getLine(), debugLoc->getColumn() };
                }

                assigns.push_back(std::make_unique<AssignTraceEvent>(
                    mdGlobal->getName(),
                    expr,
                    location
                ));
            } else if (callee->getName() == GazerIntrinsic::FunctionEntryName) {
                auto diSP = dyn_cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                assigns.push_back(std::make_unique<FunctionEntryEvent>(
                    diSP->getName()
                ));
            } else if (callee->getName().startswith(GazerIntrinsic::FunctionReturnValuePrefix)) {
                auto diSP = dyn_cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                auto value = call->getArgOperand(1);
                auto lit = getLiteralFromValue(value, model);
                if (lit == nullptr) {
                    continue;
                }

                assigns.push_back(std::make_unique<FunctionReturnEvent>(
                    diSP->getName(),
                    lit
                ));
            } else if (callee->getName() == GazerIntrinsic::FunctionReturnVoidName) {
                auto diSP = dyn_cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                assigns.push_back(std::make_unique<FunctionReturnEvent>(
                    diSP->getName(),
                    nullptr
                ));
            } else if (callee->getName() == GazerIntrinsic::FunctionCallReturnedName) {
                auto diSP = dyn_cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                assigns.push_back(std::make_unique<FunctionEntryEvent>(
                    diSP->getName()
                ));
            } else if (callee->isDeclaration() && !call->getType()->isVoidTy()) {
                // This a function call to a nondetermistic function.
                auto varIt = mValueMap.find(call);
                assert(varIt != mValueMap.end() && "Call results should be present in the value map");

                ExprRef<AtomicExpr> expr;

                auto exprIt = model.find(varIt->second);
                if (exprIt != model.end()) {
                    expr = exprIt->second;
                } else {
                    // For variables which are assigned but never read,
                    // it is possible to be not present in the model.
                    // For these variables, we are going insert an UndefExpr.
                    expr = UndefExpr::Get(varIt->second->getType());
                }

                LocationInfo location = {0, 0};
                if (call->getDebugLoc()) {
                    location = {
                        call->getDebugLoc()->getLine(),
                        call->getDebugLoc()->getColumn()
                    };
                }

                assigns.push_back(std::make_unique<FunctionCallEvent>(
                    callee->getName(),
                    expr,
                    std::vector<ExprRef<AtomicExpr>>(),
                    location
                ));
            }
        }
    }

    return assigns;
}


ExprRef<AtomicExpr> LLVMTraceBuilderImpl::getLiteralFromValue(llvm::Value* value, Valuation& model)
{
    auto result = mValueMap.find(value);
    
    if (result == mValueMap.end()) {
        if (llvm::isa<UndefValue>(value)) {
            // TODO: We should return the value of the corresponding undef here.
            return nullptr;
        } else if (auto cd = dyn_cast<ConstantData>(value)) {
            return LiteralFromLLVMConst(mContext, cd);
        }
    } else {
        Variable* variable = result->second;
        auto exprResult = model.find(variable);

        if (exprResult == model.end()) {
            // TODO: The expression was not found in the model, perhaps this should be an error?
            return UndefExpr::Get(variable->getType());
        }
        
        ExprRef<LiteralExpr> expr = exprResult->second;

        return expr;
    }

    return nullptr;
}

#endif