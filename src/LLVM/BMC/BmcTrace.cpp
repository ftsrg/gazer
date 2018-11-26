#include "gazer/LLVM/BMC/BmcTrace.h"

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

    while (hasParent) {
        traceBlocks.push_back(current);

        auto predRes = mPreds.find(current);
        if (predRes != mPreds.end()) {
            size_t predId;
            if (auto ci = llvm::dyn_cast<llvm::ConstantInt>(predRes->second)) {
                predId = ci->getLimitedValue();
            } else {
                auto varRes = mValueMap.find(predRes->second);
                assert(varRes != mValueMap.end()
                    && "Pred variables should be in the variable map");
                
                auto exprRes = model.find(varRes->second);
                assert(exprRes != model.end()
                    && "Pred values should be present in the model");
                
                auto lit = llvm::dyn_cast<BvLiteralExpr>(exprRes->second.get());
                predId = lit->getValue().getLimitedValue();

            }

            //current->printAsOperand(llvm::errs());
            //llvm::errs() << " PRED " << predId << "\n";
            current = mTopo[predId];
        } else {
            hasParent = false;
        }
    }

    std::reverse(traceBlocks.begin(), traceBlocks.end());
    
    for (BasicBlock* bb : traceBlocks) {
        for (Instruction& instr : *bb) {
            llvm::CallInst* call = llvm::dyn_cast<llvm::CallInst>(&instr);
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

                    auto result = mValueMap.find(value);
                    
                    if (result == mValueMap.end()) {
                        if (llvm::isa<UndefValue>(value)) {
                            continue;
                        } else if (auto cd = dyn_cast<ConstantData>(value)) {
                            auto lit = LiteralFromLLVMConst(cd);

                            assigns.push_back(std::make_unique<AssignTraceEvent>(
                                diVar->getName(),
                                lit,
                                location
                            ));
                        }
                    } else {
                        Variable* variable = result->second;
                        auto exprResult = model.find(variable);

                        if (exprResult == model.end()) {
                            continue;
                        }
                        
                        std::shared_ptr<LiteralExpr> expr = exprResult->second;

                        assigns.emplace_back(new AssignTraceEvent(
                            diVar->getName(),
                            exprResult->second,
                            location
                        ));
                    }
                }
            } else if (callee->getName() == "gazer.inlined_global.write") {
                auto mdValue = cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata();
                auto value = cast<ValueAsMetadata>(mdValue)->getValue();

                auto mdGlobal = dyn_cast<DIGlobalVariable>(
                    cast<MetadataAsValue>(call->getArgOperand(1))->getMetadata()
                );

                std::shared_ptr<LiteralExpr> expr = nullptr;
                if (auto ci = dyn_cast<ConstantInt>(value)) {
                    expr = BvLiteralExpr::get(
                        BvType::get(ci->getBitWidth()),
                        ci->getValue()
                    );
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
                if (auto debugLoc = call->getDebugLoc()) {
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
