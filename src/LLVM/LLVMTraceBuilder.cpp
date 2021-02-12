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
#include "gazer/LLVM/LLVMTraceBuilder.h"
#include "gazer/LLVM/Automaton/ModuleToAutomata.h"
#include "gazer/Core/Expr/ExprEvaluator.h"
#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/raw_ostream.h>

using namespace gazer;
using namespace llvm;

static void updateCurrentValuation(Valuation& val, const std::vector<VariableAssignment>& action)
{
    for (auto& assign : action) {
        if (assign.getValue()->getKind() == Expr::Undef) {
            // Ignore undef's at this point -- they are not required.
            continue;
        }

        assert(assign.getValue()->getKind() == Expr::Literal);
        val[assign.getVariable()] = boost::static_pointer_cast<LiteralExpr>(assign.getValue());
    }
}

gazer::Type* LLVMTraceBuilder::preferredTypeFromDIType(llvm::DIType* diTy)
{
    assert(diTy != nullptr);

    if (auto basicTy = dyn_cast<llvm::DIBasicType>(diTy)) {
        switch (basicTy->getEncoding()) {
            case dwarf::DW_ATE_boolean:
                return &BoolType::Get(mContext);
            case dwarf::DW_ATE_signed:
            case dwarf::DW_ATE_signed_char:
            case dwarf::DW_ATE_unsigned:
            case dwarf::DW_ATE_unsigned_char:
                return &BvType::Get(mContext, diTy->getSizeInBits());
            case dwarf::DW_ATE_float:
                switch (diTy->getSizeInBits()) {
                    case 32: return &FloatType::Get(mContext, FloatType::Single);
                    case 64: return &FloatType::Get(mContext, FloatType::Double);
                    default:
                        break;
                }
            default:
               break;
        }
    }

    return nullptr;
}

ExprRef<AtomicExpr> LLVMTraceBuilder::getLiteralFromLLVMConst(
    const llvm::ConstantData* value, gazer::Type* preferredType
) {
    if (auto ci = llvm::dyn_cast<llvm::ConstantInt>(value)) {
        unsigned width = ci->getType()->getIntegerBitWidth();
        return BvLiteralExpr::Get(BvType::Get(mContext, width), ci->getValue());
    }
    
    if (auto cfp = llvm::dyn_cast<llvm::ConstantFP>(value)) {
        assert((!preferredType || preferredType->isFloatType()) && "Non-float preferred for a float value!");

        auto fltTy = cfp->getType();
        FloatType::FloatPrecision precision;
        if (fltTy->isHalfTy()) {
            precision = FloatType::Half;
        } else if (fltTy->isFloatTy()) {
            precision = FloatType::Single;
        } else if (fltTy->isDoubleTy()) {
            precision = FloatType::Double;
        } else if (fltTy->isFP128Ty()) {
            precision = FloatType::Quad;
        } else {
            llvm_unreachable("Unsupported floating-point type.");
        }

        return FloatLiteralExpr::Get(FloatType::Get(mContext, precision), cfp->getValueAPF());
    }

    return UndefExpr::Get(BvType::Get(mContext, 1));
}

auto LLVMTraceBuilder::build(
    std::vector<Location*>& states,
    std::vector<std::vector<VariableAssignment>>& actions) -> std::unique_ptr<Trace>
{
    assert(states.size() == actions.size() + 1);
    assert(states.front() == states.front()->getAutomaton()->getEntry());

    std::vector<std::unique_ptr<TraceEvent>> events;
    llvm::DenseSet<const llvm::Value*> undefs;

    Valuation currentVals;
    ValuationExprEvaluator evaluator(currentVals);

    auto shouldProcessEntry = [](CfaToLLVMTrace::BlockToLocationInfo info) -> bool {
        return info.block != nullptr && info.kind == CfaToLLVMTrace::Location_Entry;
    };

    auto stateIt  = states.begin();
    auto stateEnd = states.end();

    auto actionIt = actions.begin();
    auto actionEnd = actions.end();

    const BasicBlock* prevBB = nullptr;
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
        updateCurrentValuation(currentVals, *actionIt);
        const BasicBlock* bb = entry.block;

        for (const llvm::Instruction& inst : *bb) {
            // Passing through this instruction means that it's possibly not undefined anymore.
            // Remove it from the undefined set, and place it back again if it turns out to be undef.
            undefs.erase(&inst);

            // See if we have reads on undefined values.
            for (unsigned i = 0; i < inst.getNumOperands(); ++i) {
                llvm::Value* operand = inst.getOperand(i);
                if (undefs.count(operand) != 0) {
                    // We have passed through an use of an undefined value.

                    if (auto phi = dyn_cast<PHINode>(&inst)) {
                        // If the instruction is a PHI node, having the
                        // undef value as an operand is still not UB. However,
                        // if we are actually getting undef from the phi,
                        // register it into the undef set.
                        if (phi->getIncomingBlock(i) == prevBB) {
                            undefs.insert(phi);
                        }
                    } else if (auto select = dyn_cast<SelectInst>(&inst)) {
                        if (i == 0) {
                            // The condition is undefined, this UB
                            auto expr = mCfaToLlvmTrace.getExpressionForValue(loc->getAutomaton(), &inst);
                            events.push_back(std::make_unique<UndefinedBehaviorEvent>(
                                evaluator.evaluate(expr)
                                // TODO: Add location
                            ));
                        } else {
                            auto expr = this->getLiteralFromValue(loc->getAutomaton(), inst.getOperand(0), currentVals);

                            // We get undefined value in two cases:
                            //  1) if the condition is true and the use is the 'then' value, or
                            //  2) if the condition is false and the use is the 'else' value.
                            bool isUB = (i == 1 && expr == BoolLiteralExpr::True(mContext));
                            isUB |= (i == 2 && expr == BoolLiteralExpr::False(mContext)) ;
                            
                            if (isUB) {
                                undefs.insert(&inst);
                            }
                        }
                    } else if (!llvm::isa<IntrinsicInst>(inst)) {
                        auto expr = mCfaToLlvmTrace.getExpressionForValue(loc->getAutomaton(), operand);
                        events.push_back(std::make_unique<UndefinedBehaviorEvent>(
                            evaluator.evaluate(expr)
                            // TODO: Add location
                        ));
                    }
                }
            }

            auto call = dyn_cast<CallInst>(&inst);
            if (call == nullptr || call->getCalledFunction() == nullptr) {
                continue;
            }

            llvm::Function* callee = call->getCalledFunction();
            if (auto dvi = llvm::dyn_cast<llvm::DbgValueInst>(&inst)) {
                this->handleDbgValueInst(loc, dvi, events, currentVals);
            } else if (callee->getName().startswith(GazerIntrinsic::InlinedGlobalWritePrefix)) {
                auto value = call->getArgOperand(0);
                auto mdGlobal = cast<DIGlobalVariable>(
                    cast<MetadataAsValue>(call->getArgOperand(1))->getMetadata()
                );

                auto lit = this->getLiteralFromValue(loc->getAutomaton(), value, currentVals);

                LocationInfo location = { 0, 0 };
                if (auto& debugLoc = call->getDebugLoc()) {
                    location = { debugLoc->getLine(), debugLoc->getColumn() };
                }

                events.push_back(std::make_unique<AssignTraceEvent>(
                    traceVarFromDIVar(mdGlobal),
                    lit,
                    location
                ));
            } else if (callee->getName().startswith(GazerIntrinsic::FunctionEntryPrefix)) {
                auto diSP = cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                std::vector<ExprRef<AtomicExpr>> args;
                for (size_t i = 1; i < call->getNumArgOperands(); ++i) {
                    args.push_back(
                        this->getLiteralFromValue(loc->getAutomaton(), call->getArgOperand(i), currentVals)
                    );
                }

                events.push_back(std::make_unique<FunctionEntryEvent>(
                    diSP->getName().str(),
                    args
                ));
            } else if (callee->getName() == GazerIntrinsic::FunctionReturnVoidName) {
                auto diSP = cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                events.push_back(std::make_unique<FunctionReturnEvent>(
                    diSP->getName().str(),
                    nullptr
                ));
            } else if (callee->getName().startswith(GazerIntrinsic::FunctionReturnValuePrefix))  {
                auto diSP = cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                auto expr = this->getLiteralFromValue(loc->getAutomaton(), call->getArgOperand(1), currentVals);
                events.push_back(std::make_unique<FunctionReturnEvent>(
                    diSP->getName().str(),
                    expr
                ));
            } else if (callee->getName() == GazerIntrinsic::FunctionCallReturnedName) {
                auto diSP = cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                events.push_back(std::make_unique<FunctionEntryEvent>(
                    diSP->getName().str()
                ));
            } else if (callee->getName().startswith("gazer.undef_value.")) {
                // Register that we have passed through an undef value.
                // Note that this does not necessarily mean undefined behavior,
                // as UB only occurs if we actually read this value.
                undefs.insert(call);
            } else if (callee->getName().startswith("gazer.dummy")) {
                // These functions are the part of an "invisible" instrumentation and should be ignored.
            } else if (callee->getName().startswith(GazerIntrinsic::NoOverflowPrefix)) {
               // Do not show the intrinsic functions here
               // TODO: We could add some information about the overflow (e.g. the unrepresentable value).
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
                    callee->getName().str(),
                    expr,
                    std::vector<ExprRef<AtomicExpr>>(),
                    location
                ));
            }
        }

        prevBB = bb;
        ++stateIt;
        ++actionIt;
    }

    return std::make_unique<Trace>(std::move(events));
}

void LLVMTraceBuilder::handleDbgValueInst(
    const Location* loc,
    const llvm::DbgValueInst* dvi,
    std::vector<std::unique_ptr<TraceEvent>>& events,
    Valuation& currentVals
) {
    if (dvi->getValue() != nullptr && dvi->getVariable() != nullptr) {
        Value* value = dvi->getValue();
        DILocalVariable* diVar = dvi->getVariable();

        DebugLoc debugLoc = nullptr;
        LocationInfo location = { 0, 0 };
        if (auto valInst = dyn_cast<Instruction>(value)) {
            if (valInst->getDebugLoc()) {
                debugLoc = valInst->getDebugLoc();
            }
        }

        if (!debugLoc && dvi->getDebugLoc()) {
            debugLoc = dvi->getDebugLoc();
        }

        if (debugLoc) {
            location = { debugLoc->getLine(), debugLoc->getColumn() };
        }

        DIType* diType = diVar->getType();
        gazer::Type* preferredType = this->preferredTypeFromDIType(diType);

        auto lit = getLiteralFromValue(loc->getAutomaton(), value, currentVals, preferredType);
        events.push_back(std::make_unique<AssignTraceEvent>(
            traceVarFromDIVar(diVar),
            lit,
            location
        ));
    }
}

ExprRef<AtomicExpr> LLVMTraceBuilder::getLiteralFromValue(
    Cfa* cfa, const llvm::Value* value, Valuation& model, gazer::Type* preferredType
) {
    if (auto cd = dyn_cast<ConstantData>(value)) {
        return this->getLiteralFromLLVMConst(cd, preferredType);
    }

    ValuationExprEvaluator eval(model);

    auto expr = mCfaToLlvmTrace.getExpressionForValue(cfa, value);
    if (expr != nullptr) {
        auto ret = eval.evaluate(expr);
        if (ret != nullptr) {
            return ret;
        }
    }

    // This is possible if the assignment was not required
    // by the model checking engine to produce a counterexample
    // or if the value was stripped away by an optimization.
    // Insert an undef expression to mark this case.
    // XXX: Currently we are using a dummy 1-bit Bv type, this should be updated.
    return UndefExpr::Get(BvType::Get(mContext, 1));
}

TraceVariable LLVMTraceBuilder::traceVarFromDIVar(const llvm::DIVariable* diVar)
{
    llvm::DIType* diType = diVar->getType();
    TraceVariable::Representation rep = TraceVariable::Rep_Unknown;

    if (auto basicDiType = dyn_cast<DIBasicType>(diType)) {
        switch (basicDiType->getEncoding()) {
            case dwarf::DW_ATE_boolean: rep = TraceVariable::Rep_Bool; break;
            case dwarf::DW_ATE_signed: rep = TraceVariable::Rep_Signed; break;
            case dwarf::DW_ATE_unsigned: rep = TraceVariable::Rep_Unsigned; break;
            case dwarf::DW_ATE_signed_char: rep = TraceVariable::Rep_Signed; break;
            case dwarf::DW_ATE_unsigned_char: rep = TraceVariable::Rep_Char; break;
            case dwarf::DW_ATE_float: rep = TraceVariable::Rep_Float; break;
            default:
                break;
        }
    }

    return TraceVariable(diVar->getName().str(), rep, diType->getSizeInBits());
}

