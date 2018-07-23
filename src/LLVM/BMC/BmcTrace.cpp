#include "gazer/LLVM/BMC/BmcTrace.h"

#include <llvm/IR/DebugInfoMetadata.h>

#include <llvm/Support/raw_ostream.h>

using namespace gazer;
using namespace llvm;

void BmcTrace::AssignmentEvent::write(BmcTraceWriter& writer)
{
    writer.writeEvent(*this);
}

void BmcTrace::FunctionEntryEvent::write(BmcTraceWriter& writer)
{
    writer.writeEvent(*this);
}

namespace
{

class TextBmcTraceWriter : public BmcTraceWriter
{
    size_t mFuncEntries = 0;
public:
    TextBmcTraceWriter(llvm::raw_ostream& os, bool printBV = true)
        : BmcTraceWriter(os), mPrintBV(printBV)
    {}

public:
    void writeEvent(BmcTrace::AssignmentEvent& event) override {
        std::shared_ptr<LiteralExpr> expr = event.getExpr();

        mOS << "  ";
        mOS << event.getVariableName() << " := ";
        event.getExpr()->print(mOS);
        if (expr->getType().isIntType() && mPrintBV) {
            auto apVal = llvm::dyn_cast<IntLiteralExpr>(expr.get())->getValue();
            std::bitset<64> bits(apVal.getLimitedValue());
            mOS << "\t(0b" << bits.to_string() << ")";
        }
        auto location = event.getLocation();
        if (location.getLine() != 0) {
            mOS << "\t at "
                << location.getLine()
                << ":"
                << location.getColumn()
                << "";
        }
        mOS << "\n";
    };

    void writeEvent(BmcTrace::FunctionEntryEvent& event) override {
        mOS << "#" << (mFuncEntries++)
            << " in function " << event.getFunctionName() << ":\n";
    }
private:
    bool mPrintBV;
};

}

namespace gazer { namespace bmc {
    std::unique_ptr<BmcTraceWriter> CreateTextTraceWriter(llvm::raw_ostream& os) {
        return std::make_unique<TextBmcTraceWriter>(os);
    }
}}

BmcTrace BmcTrace::Create(
    TopologicalSort& topo,
    llvm::DenseMap<llvm::BasicBlock*, size_t>& blocks,
    llvm::DenseMap<llvm::BasicBlock*, llvm::Value*>& preds,
    llvm::BasicBlock* errorBlock,
    Valuation& model,
    const InstToExpr::ValueToVariableMapT& valueMap)
{
    std::vector<std::unique_ptr<BmcTrace::Event>> assigns;
    std::vector<BasicBlock*> traceBlocks;

    bool hasParent = true;
    BasicBlock* current = errorBlock;

    while (hasParent) {
        traceBlocks.push_back(current);

        auto predRes = preds.find(current);
        if (predRes != preds.end()) {
            size_t predId;
            if (auto ci = llvm::dyn_cast<llvm::ConstantInt>(predRes->second)) {
                predId = ci->getLimitedValue();
            } else {
                auto varRes = valueMap.find(predRes->second);
                assert(varRes != valueMap.end()
                    && "Pred variables should be in the variable map");
                
                auto exprRes = model.find(varRes->second);
                assert(exprRes != model.end()
                    && "Pred values should be present in the model");
                
                auto lit = llvm::dyn_cast<IntLiteralExpr>(exprRes->second.get());
                predId = lit->getValue().getLimitedValue();

            }

            //current->printAsOperand(llvm::errs());
            //llvm::errs() << " PRED " << predId << "\n";
            current = topo[predId];
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

                    auto result = valueMap.find(value);
                    
                    if (result == valueMap.end()) {
                        // This is an unknown value for a given variable
                        //assigns.push_back({
                        //    nullptr, nullptr, value, {0, 0}, diVar->getName()
                        //});
                        continue;
                    }
                    //assert(result != valueMap.end()
                    //    && "Named values should be present in the value map");

                    Variable* variable = result->second;
                    auto exprResult = model.find(variable);

                    if (exprResult == model.end()) {
                        continue;
                    }
                    
                    std::shared_ptr<LiteralExpr> expr = exprResult->second;
                    BmcTrace::LocationInfo location = { 0, 0 };

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

                    assigns.push_back(std::make_unique<BmcTrace::AssignmentEvent>(
                        diVar->getName(),
                        exprResult->second,
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
                    expr = IntLiteralExpr::get(
                        *IntType::get(ci->getBitWidth()),
                        ci->getValue()
                    );
                } else {
                    auto result = valueMap.find(value);
                    if (result != valueMap.end()) {
                        Variable* variable = result->second;
                        auto exprResult = model.find(variable);

                        if (exprResult != model.end()) {
                            expr = exprResult->second;
                        }
                    }
                }

                BmcTrace::LocationInfo location = { 0, 0 };
                if (auto debugLoc = call->getDebugLoc()) {
                    location = { debugLoc->getLine(), debugLoc->getColumn() };
                }

                assigns.push_back(std::make_unique<BmcTrace::AssignmentEvent>(
                    mdGlobal->getName(),
                    expr,
                    location
                ));
            } else if (callee->getName() == "gazer.function.entry") {
                auto diSP = dyn_cast<DISubprogram>(
                    cast<MetadataAsValue>(call->getArgOperand(0))->getMetadata()
                );

                assigns.push_back(std::make_unique<BmcTrace::FunctionEntryEvent>(
                    diSP->getName()
                ));
            }
        }
    }

    return BmcTrace(assigns, traceBlocks);
}
