#include "gazer/LLVM/BMC/BmcTrace.h"
#include "gazer/Core/Expr/ExprEvaluator.h"

#include <llvm/IR/DebugInfoMetadata.h>

#include <llvm/Support/raw_ostream.h>
#include <gazer/LLVM/BMC/BmcTrace.h>

using namespace gazer;
using namespace llvm;

std::vector<std::unique_ptr<TraceEvent>> LLVMBmcTraceBuilder::buildEvents(Valuation &model)
{
    std::vector<std::unique_ptr<TraceEvent>> assigns;
    std::vector<BasicBlock*> traceBlocks;

    bool hasParent = true;
    BasicBlock* current = mErrorBlock;

    ExprEvaluator eval{model};

    while (hasParent) {
        traceBlocks.push_back(current);

        auto predRes = mPreds.find(current);
        if (predRes != mPreds.end()) {
            auto predLit = dyn_cast_or_null<BvLiteralExpr>(
                eval.visit(predRes->second).get()
            );
            assert((predLit != nullptr) && "Pred values should be evaluatable!");

            size_t predID = predLit->getValue().getLimitedValue();
            current = mTopo[predID];
        } else {
            hasParent = false;
        }
    }

    std::reverse(traceBlocks.begin(), traceBlocks.end());
    
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
            } else if (callee->getName() == "gazer.inlined_global.write") {
                auto mdValue = cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata();
                auto value = cast<ValueAsMetadata>(mdValue)->getValue();

                auto mdGlobal = dyn_cast<DIGlobalVariable>(
                    cast<MetadataAsValue>(call->getArgOperand(1))->getMetadata()
                );

                std::shared_ptr<LiteralExpr> expr = nullptr;
                if (auto ci = dyn_cast<ConstantInt>(value)) {
                    expr = BvLiteralExpr::Get(ci->getValue());
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
            } else if (callee->getName() == "gazer.function.entry") {
                auto diSP = dyn_cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                assigns.push_back(std::make_unique<FunctionEntryEvent>(
                    diSP->getName()
                ));
            } else if (callee->getName().startswith("gazer.function.return_value.")) {
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
            } else if (callee->getName() == "gazer.function.return_void") {
                auto diSP = dyn_cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                assigns.push_back(std::make_unique<FunctionReturnEvent>(
                    diSP->getName(),
                    nullptr
                ));
            } else if (callee->getName() == "gazer.function.call_returned") {
                auto diSP = dyn_cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                assigns.push_back(std::make_unique<FunctionEntryEvent>(
                    diSP->getName()
                ));
            } else if (callee->getName() == "gazer.function.arg") {
                //auto md = cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata();
                //auto value = cast<ValueAsMetadata>(md)->getValue();

                //auto variable = mValueMap.find(value)->second;
                //auto expr = model.find(variable)->second;

                //assigns.push_back(std::make_unique<BmcTrace::ArgumentValueEvent>(
                //    "", expr
                //));
            } else if (callee->isDeclaration() && !call->getType()->isVoidTy()) {
                // This a function call to a nondetermistic function.
                auto varIt = mValueMap.find(call);
                assert(varIt != mValueMap.end() && "Call results should be present in the value map");

                std::shared_ptr<AtomicExpr> expr;

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
                    std::vector<std::shared_ptr<AtomicExpr>>(),
                    location
                ));
            }
        }
    }

    return assigns;
}

std::shared_ptr<AtomicExpr> LLVMBmcTraceBuilder::getLiteralFromValue(llvm::Value* value, Valuation& model)
{
    auto result = mValueMap.find(value);
    
    if (result == mValueMap.end()) {
        if (llvm::isa<UndefValue>(value)) {
            // TODO: We should return the value of the corresponding undef here.
            return nullptr;
        } else if (auto cd = dyn_cast<ConstantData>(value)) {
            return LiteralFromLLVMConst(cd);
        }
    } else {
        Variable* variable = result->second;
        auto exprResult = model.find(variable);

        if (exprResult == model.end()) {
            // TODO: The expression was not found in the model, perhaps this should be an error?
            return UndefExpr::Get(variable->getType());
        }
        
        std::shared_ptr<LiteralExpr> expr = exprResult->second;

        return expr;
    }

    return nullptr;
}
