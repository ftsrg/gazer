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
        if (assign.getValue()->getKind() == Expr::Undef) {
            // Ignore undef's at this point -- they are not required.
            continue;
        }

        assert(assign.getValue()->getKind() == Expr::Literal);
        val[assign.getVariable()] = boost::static_pointer_cast<LiteralExpr>(assign.getValue());
    }
}

static bool handleUndefOperands(llvm::Instruction& inst, llvm::DenseSet<llvm::Value*> undefs, llvm::BasicBlock* prevBB)
{
    return true;
}

static ExprRef<AtomicExpr> LiteralFromLLVMConst(GazerContext& context, const llvm::ConstantData* value, bool i1AsBool = true)
{
    if (auto ci = llvm::dyn_cast<llvm::ConstantInt>(value)) {
        unsigned width = ci->getType()->getIntegerBitWidth();
        if (width == 1 && i1AsBool) {
            return BoolLiteralExpr::Get(BoolType::Get(context), ci->isZero() ? false : true);
        }

        return BvLiteralExpr::Get(BvType::Get(context, width), ci->getValue());
    }
    
    if (auto cfp = llvm::dyn_cast<llvm::ConstantFP>(value)) {
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

        return FloatLiteralExpr::Get(FloatType::Get(context, precision), cfp->getValueAPF());
    }

    return UndefExpr::Get(BvType::Get(context, 1));
}

std::unique_ptr<Trace> LLVMTraceBuilder::build(
    std::vector<Location*>& states,
    std::vector<std::vector<VariableAssignment>>& actions)
{
    assert(states.size() == actions.size() + 1);
    assert(states.front() == states.front()->getAutomaton()->getEntry());

    std::vector<std::unique_ptr<TraceEvent>> events;
    llvm::DenseSet<llvm::Value*> undefs;

    Valuation currentVals;

    auto shouldProcessEntry = [](CfaToLLVMTrace::BlockToLocationInfo info) -> bool {
        return info.block != nullptr && info.kind == CfaToLLVMTrace::Location_Entry;
    };

    auto stateIt  = states.begin();
    auto stateEnd = states.end();

    auto actionIt = actions.begin();
    auto actionEnd = actions.end();

    BasicBlock* prevBB = nullptr;
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
                    } else if (!llvm::isa<IntrinsicInst>(inst)) {
                        auto expr = mCfaToLlvmTrace.getExpressionForValue(loc->getAutomaton(), operand);
                        events.push_back(std::make_unique<UndefinedBehaviorEvent>(
                            currentVals.eval(expr)
                            // TODO: Add location
                        ));
                    }
                }
            }

            if (auto phi = dyn_cast<PHINode>(&inst)) {
                if (undefs.count(phi) != 0 && undefs.count(phi->getIncomingValueForBlock(prevBB)) == 0) {
                    // If this PHI node was previosuly registered as an undef value,
                    // but now has a different incoming value, remove it from the undef list.
                    undefs.erase(phi);
                }
            }

            auto call = dyn_cast<CallInst>(&inst);
            if (!call || call->getCalledFunction() == nullptr) {
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
                        // This is possible if the assignment was not required
                        // by the model checking engine to produce a counterexample.
                        // Insert an undef expression to mark this case.
                        // XXX: Currently we are using a dummy 1-bit Bv type, this should be updated.
                        events.push_back(std::make_unique<AssignTraceEvent>(
                            diVar->getName(),
                            UndefExpr::Get(gazer::BvType::Get(mContext, 1)),
                            location
                        ));
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
            } else if (callee->getName().startswith("gazer.undef_value.")) {
                // Register that we have passed through an undef value.
                // Note that this does not necessarily mean undefined behavior,
                // as UB only occurs if we actually read this value.
                undefs.insert(call);
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

        prevBB = bb;
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
