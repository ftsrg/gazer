#include "gazer/Trace/Trace.h"
#include "gazer/LLVM/Trace/TestHarnessGenerator.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/IR/Constants.h>

#include <gtest/gtest.h>

using namespace gazer;

TEST(TestHarnessGeneratorTest, SmokeTest1)
{
    // Build a trace
    GazerContext ctx;
    BvType& bv32Ty = BvType::Get(ctx, 32);

    std::vector<std::unique_ptr<TraceEvent>> events;
    events.emplace_back(new FunctionCallEvent("__VERIFIER_nondet_int", BvLiteralExpr::Get(bv32Ty, llvm::APInt{32, 0})));
    events.emplace_back(new FunctionCallEvent("__VERIFIER_nondet_int", BvLiteralExpr::Get(bv32Ty, llvm::APInt{32, 1})));
    events.emplace_back(new FunctionCallEvent("__VERIFIER_nondet_int", BvLiteralExpr::Get(bv32Ty, llvm::APInt{32, 2})));
    events.emplace_back(new FunctionCallEvent("__VERIFIER_nondet_int", BvLiteralExpr::Get(bv32Ty, llvm::APInt{32, 3})));

    auto trace = new Trace(std::move(events));

    llvm::LLVMContext llvmContext;
    auto module = std::make_unique<llvm::Module>("test1", llvmContext);
    auto llvmInt32Ty = llvm::IntegerType::getInt32Ty(llvmContext);
    module->getOrInsertFunction("__VERIFIER_nondet_int", llvm::FunctionType::get(llvmInt32Ty, /*isVarArg=*/false));
    
    // Generate the harness
    auto harness = GenerateTestHarnessModuleFromTrace(*trace, llvmContext, *module);

    auto func = harness->getFunction("__VERIFIER_nondet_int");

    llvm::errs() << *harness << "\n";

    ASSERT_TRUE(func != nullptr);

    auto values = harness->getGlobalVariable("gazer.trace_value.__VERIFIER_nondet_int", /*allowInternal=*/true);
    ASSERT_TRUE(values != nullptr);

    auto counter = harness->getGlobalVariable("gazer.trace_counter.__VERIFIER_nondet_int", /*allowInternal=*/true);
    ASSERT_TRUE(counter != nullptr);

    auto ca = llvm::ConstantArray::get(llvm::ArrayType::get(llvmInt32Ty, 4), {
        llvm::ConstantInt::get(llvmInt32Ty, llvm::APInt{32, 0}),
        llvm::ConstantInt::get(llvmInt32Ty, llvm::APInt{32, 1}),
        llvm::ConstantInt::get(llvmInt32Ty, llvm::APInt{32, 2}),
        llvm::ConstantInt::get(llvmInt32Ty, llvm::APInt{32, 3})
    });

    ASSERT_EQ(values->getInitializer(), ca);
}
