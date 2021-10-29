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
#include "gazer/LLVM/Trace/TestHarnessGenerator.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/IRBuilder.h>

#include <unordered_map>

using namespace gazer;
using namespace llvm;

static llvm::Constant* exprToLLVMValue(ExprRef<AtomicExpr>& expr, LLVMContext& context, llvm::Type* targetTy)
{
    // For bitvectors, integers, booleans we want integer as the target type.
    assert(targetTy->isIntegerTy()
        || (!expr->getType().isBvType() && !expr->getType().isIntType() && !expr->getType().isBoolType())
        && "Bitvectors, integers, and booleans may only have integer as their target type!"
    );

    if (expr->getKind() == Expr::Undef) {
        return llvm::UndefValue::get(targetTy);
    }

    if (auto bvLit = llvm::dyn_cast<BvLiteralExpr>(expr)) {
        return llvm::ConstantInt::get(context, bvLit->getValue());
    } else if (auto intLit = llvm::dyn_cast<IntLiteralExpr>(expr)) {
        return llvm::ConstantInt::get(context, llvm::APInt{targetTy->getIntegerBitWidth(), static_cast<uint64_t>(intLit->getValue())});
    } else if (auto boolLit = llvm::dyn_cast<BoolLiteralExpr>(expr)) {
        if (boolLit->getValue()) {
            return llvm::ConstantInt::getTrue(context);
        }
        
        return llvm::ConstantInt::getFalse(context);
    } else if (auto fltLit = llvm::dyn_cast<FloatLiteralExpr>(expr)) {
        return llvm::ConstantFP::get(context, fltLit->getValue());    
    }
    
    llvm_unreachable("Unsupported expression kind");
}

std::unique_ptr<Module> gazer::GenerateTestHarnessModuleFromTrace(
    const Trace& trace, LLVMContext& context, const llvm::Module& llvmModule
) {
    const DataLayout& dl = llvmModule.getDataLayout();

    std::unordered_map<llvm::Function*, std::vector<ExprRef<AtomicExpr>>> calls;
    for (auto& event : trace) {
        if (event->getKind() == TraceEvent::Event_FunctionCall) {
            auto callEvent = llvm::cast<FunctionCallEvent>(event.get());

            llvm::Function* callee = llvmModule.getFunction(callEvent->getFunctionName());
            assert(callee != nullptr && "The function declaration should be present in the module");
            auto expr = callEvent->getReturnValue();

            calls[callee].push_back(expr);
        }
    }

    auto test = std::make_unique<Module>("test", context);
    test->setDataLayout(dl);

    for (auto& [function, vec] : calls) {
        auto retTy = function->getReturnType();
        llvm::SmallVector<llvm::Constant*, 10> values;
        std::transform(vec.begin(), vec.end(), std::back_inserter(values),
            [&context, retTy](auto& expr) { return exprToLLVMValue(expr, context, retTy); }
        );

        auto arrTy = llvm::ArrayType::get(retTy, values.size());
        auto array = llvm::ConstantArray::get(arrTy, values);

        auto valueArray = new GlobalVariable(
            *test, arrTy, true, GlobalValue::PrivateLinkage, array, "gazer.trace_value." + function->getName()
        );

        // Create a global variable counting the calls to this function
        auto counterTy = llvm::Type::getInt32Ty(context);
        auto counter = new GlobalVariable(
            *test, counterTy, false, GlobalValue::PrivateLinkage,
            llvm::ConstantInt::get(counterTy, 0),
            "gazer.trace_counter." + function->getName()
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
