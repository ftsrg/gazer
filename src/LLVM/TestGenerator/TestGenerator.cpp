#include "gazer/LLVM/TestGenerator/TestGenerator.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/IRBuilder.h>

#include <unordered_map>

using namespace gazer;
using namespace llvm;

static llvm::Constant* exprToLLVMValue(std::shared_ptr<LiteralExpr>& expr, LLVMContext& context)
{
    if (expr->getType().isIntType()) {
        auto intLit = llvm::cast<IntLiteralExpr>(expr.get());
        return llvm::ConstantInt::get(context, intLit->getValue());
    } else if (expr->getType().isBoolType()) {
        auto boolLit = llvm::cast<BoolLiteralExpr>(expr.get());
        if (boolLit->getValue()) {
            return llvm::ConstantInt::getTrue(context);
        } else {
            return llvm::ConstantInt::getFalse(context);
        }
    } else if (expr->getType().isFloatType()) {
        auto fltLit = llvm::cast<FloatLiteralExpr>(expr.get());
        return llvm::ConstantFP::get(context, fltLit->getValue());    
    } else {
        assert(false && "Unsupported expression kind");
    }
}

std::unique_ptr<Module> TestGenerator::generateModuleFromTrace(
    BmcTrace& trace, LLVMContext& context, const DataLayout& dl
) {
    std::unordered_map<llvm::Function*, std::vector<std::shared_ptr<LiteralExpr>>> calls;
    for (auto& event : trace) {
        if (event->getKind() == BmcTrace::Event::FunctionCall) {
            auto callEvent = llvm::cast<BmcTrace::FunctionCallEvent>(event.get());

            llvm::Function* callee = callEvent->getFunction();
            auto expr = callEvent->getReturnValue();

            calls[callee].push_back(expr);
        }
    }

    std::unique_ptr<Module> test = std::make_unique<Module>("test", context);
    test->setDataLayout(dl);

    for (auto& pair : calls) {
        llvm::Function* function = pair.first;
        std::vector<std::shared_ptr<LiteralExpr>>& vec = pair.second;

        llvm::SmallVector<llvm::Constant*, 10> values;
        std::transform(vec.begin(), vec.end(), std::back_inserter(values),
            [&context](auto& expr) { return exprToLLVMValue(expr, context); }
        );

        auto arrTy = llvm::ArrayType::get(function->getReturnType(), values.size());
        auto array = llvm::ConstantArray::get(arrTy, values);

        llvm::GlobalVariable* valueArray = new GlobalVariable(
            *test, arrTy, true, GlobalValue::PrivateLinkage, array
        );

        // Create a global variable counting the calls to this function
        auto counterTy = llvm::Type::getInt32Ty(context);
        llvm::GlobalVariable* counter = new GlobalVariable(
            *test, counterTy, false, GlobalValue::PrivateLinkage,
            llvm::ConstantInt::get(counterTy, 0)
        );

        llvm::Function* testFun = Function::Create(
            function->getFunctionType(),
            GlobalValue::ExternalLinkage,
            function->getName(),
            test.get()
        );

        // Create the test function body
        BasicBlock* entryBlock = BasicBlock::Create(context, "entry", testFun);
        IRBuilder<> builder(entryBlock);

        llvm::Value* loadCounter = builder.CreateLoad(counter);
        builder.CreateStore(
            builder.CreateAdd(loadCounter, ConstantInt::get(counterTy, 1)),
            counter
        );

        llvm::Value* gep = builder.CreateInBoundsGEP(
            valueArray,
            { ConstantInt::get(counterTy, 0), loadCounter }
        );
        builder.CreateRet(builder.CreateLoad(gep));
    }

    return test;
}
