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
#include "gazer/LLVM/Automaton/InstToExpr.h"

#include "gazer/LLVM/Memory/MemoryModel.h"
#include "gazer/Core/Expr/ExprEvaluator.h"

#include <llvm/IR/IRBuilder.h>

#include <gtest/gtest.h>

using namespace gazer;
using namespace llvm;

namespace
{

class InstToExprTest : public ::testing::Test
{
public:
    class InstToExprImpl : public InstToExpr
    {
    public:
        InstToExprImpl(
            llvm::Function& func,
            ExprBuilder& builder,
            LLVMTypeTranslator& types,
            MemoryInstructionHandler& memoryInstHandler,
            const LLVMFrontendSettings& settings,
            llvm::DenseMap<llvm::Value*, Variable*> vars)
            : InstToExpr(func, builder, types, memoryInstHandler, settings), mVars(std::move(vars))
        {}

        Variable* getVariable(ValueOrMemoryObject value) override
        {
            return mVars.lookup(value.asValue());
        }

    private:
        llvm::DenseMap<llvm::Value*, Variable*> mVars;
    };

public:
    GazerContext context;
    llvm::LLVMContext llvmContext;
    LLVMFrontendSettings settings;
    std::unique_ptr<ExprBuilder> builder;
    std::unique_ptr<MemoryModel> memoryModel;
    std::unique_ptr<LLVMTypeTranslator> types;

    std::unique_ptr<llvm::Module> module;
    llvm::Function* function;
    llvm::BasicBlock* startBB;
    std::unique_ptr<llvm::IRBuilder<>> ir;

    llvm::StringMap<llvm::Value*> insertedValues;
    llvm::DenseMap<llvm::Value*, Variable*> variables;

public:
    InstToExprTest() : builder(CreateExprBuilder(context)) {}

    void SetUp() override;

    void TearDown() override
    {
        module.reset();
        function = nullptr;
        startBB = nullptr;
    }

    std::unique_ptr<InstToExprImpl> createImpl()
    {
        return std::make_unique<InstToExprImpl>(
            *function, *builder, *types, memoryModel->getMemoryInstructionHandler(*function),
            settings, variables);
    }

    // Test helpers
    //==--------------------------------------------------------------------==//
public:
    llvm::Value* llvmVal(const std::string& name, llvm::Type* llvmType, gazer::Type& gazerType) {
        static int tempIdx = 0;

        auto gv = module->getOrInsertGlobal(name, llvmType);
        auto load = ir->CreateLoad(gv, "load_" + name);
        insertedValues[name] = load;

        auto variable = context.createVariable(name + std::to_string(tempIdx++), gazerType);
        variables[load] = variable;

        return load;
    }

    llvm::Value* i1Val(const std::string& name, gazer::Type& gazerType) {
        return llvmVal(name, llvm::IntegerType::getInt1Ty(llvmContext), gazerType);
    }

    llvm::Value* i32Val(const std::string& name, gazer::Type& gazerType) {
        return llvmVal(name, llvm::IntegerType::getInt32Ty(llvmContext), gazerType);
    }

    Variable* variableFor(const std::string& name) {
        auto value = insertedValues.lookup(name);
        return variables.lookup(value);
    }

    ExprPtr transformWithImpl(llvm::Value* value, gazer::Type& expectedType) {
        return createImpl()->transform(*llvm::cast<Instruction>(value), expectedType);
    }

    ExprRef<AtomicExpr> evalInt(const ExprPtr& expr, std::initializer_list<std::pair<llvm::Value*, int64_t>> vals) {
        auto vb = Valuation::CreateBuilder();
        for (auto& [llvmValue, lit] : vals) {
            vb.put(variables.lookup(llvmValue), builder->IntLit(lit));
        }

        auto valuation = vb.build();
        ValuationExprEvaluator eval(valuation);

        return eval.evaluate(expr);
    }


    template<class IrBuilderFuncTy, class ExprBuilderFuncTy>
    struct TestTriple
    {
        std::function<IrBuilderFuncTy> irBuilder;
        std::function<ExprBuilderFuncTy> exprBuilder;
        gazer::Type& expectedType;

        TestTriple(
            const std::function<IrBuilderFuncTy>& irBuilder,
            const std::function<ExprBuilderFuncTy>& exprBuilder,
            gazer::Type& expectedType)
            : irBuilder(irBuilder), exprBuilder(exprBuilder), expectedType(expectedType)
        {}
    };

    template<class IrBuilderFuncTy, class ExprBuilderFuncTy>
    using TestVector = std::vector<TestTriple<IrBuilderFuncTy, ExprBuilderFuncTy>>;

    template<class IrBuilderFuncTy, class ExprBuilderFuncTy>
    ::testing::AssertionResult
        checkBinaryOperators(const TestVector<IrBuilderFuncTy, ExprBuilderFuncTy>& tests);
};

void InstToExprTest::SetUp()
{
    module.reset(new llvm::Module("InstToExprModule", llvmContext));
    memoryModel = CreateHavocMemoryModel(context);

    types.reset(new LLVMTypeTranslator(memoryModel->getMemoryTypeTranslator(), settings));

    auto funcTy = llvm::FunctionType::get(
        llvm::Type::getVoidTy(llvmContext),
        /*isVarArg=*/false);
    function = Function::Create(funcTy, Function::ExternalLinkage, "", module.get());
    startBB = BasicBlock::Create(llvmContext, "", function);

    ir.reset(new IRBuilder<>(startBB));
}

template<class IrBuilderFuncTy, class ExprBuilderFuncTy>
auto InstToExprTest::checkBinaryOperators(const TestVector<IrBuilderFuncTy, ExprBuilderFuncTy>& tests)
    -> ::testing::AssertionResult
{
    for (unsigned i = 0; i < tests.size(); ++i) {
        auto& triple = tests[i];
        llvm::Value* inst = triple.irBuilder(i32Val("x", triple.expectedType), i32Val("y", triple.expectedType));
        ExprPtr expected = triple.exprBuilder(variableFor("x")->getRefExpr(), variableFor("y")->getRefExpr());

        auto actual = transformWithImpl(inst, triple.expectedType);

        if (expected != actual) {
            std::string buffer;
            llvm::raw_string_ostream rso(buffer);
            rso << "In test vector element #" << i << ": expected "
                << *expected << " actual " << *actual << "\n";

            return ::testing::AssertionFailure() << rso.str();
        }
    }

    return ::testing::AssertionSuccess();
}

#define BINARY_IR_FUNC(FUNCNAME)                                                                  \
    [this](llvm::Value* lhs, llvm::Value* rhs) { return ir->FUNCNAME(lhs, rhs); }

#define BINARY_EXPR_FUNC(FUNCNAME)                                                                \
    [this](const ExprPtr& lhs, const ExprPtr& rhs) { return builder->FUNCNAME(lhs, rhs); }

TEST_F(InstToExprTest, TransformBinaryBvArithmeticOperator)
{
    settings.ints = IntRepresentation::BitVectors;

    auto& bv32Ty = BvType::Get(context, 32);

    TestVector<llvm::Value*(llvm::Value*, llvm::Value*), ExprPtr(ExprPtr, ExprPtr)> tests = {
        {BINARY_IR_FUNC(CreateAdd), BINARY_EXPR_FUNC(Add),    bv32Ty},
        {BINARY_IR_FUNC(CreateSub), BINARY_EXPR_FUNC(Sub),    bv32Ty},
        {BINARY_IR_FUNC(CreateMul), BINARY_EXPR_FUNC(Mul),    bv32Ty},
        {BINARY_IR_FUNC(CreateSDiv), BINARY_EXPR_FUNC(BvSDiv), bv32Ty},
        {BINARY_IR_FUNC(CreateUDiv), BINARY_EXPR_FUNC(BvUDiv), bv32Ty},
        {BINARY_IR_FUNC(CreateSRem), BINARY_EXPR_FUNC(BvSRem), bv32Ty},
        {BINARY_IR_FUNC(CreateURem), BINARY_EXPR_FUNC(BvURem), bv32Ty},
        {BINARY_IR_FUNC(CreateShl), BINARY_EXPR_FUNC(Shl),    bv32Ty},
        {BINARY_IR_FUNC(CreateLShr), BINARY_EXPR_FUNC(LShr),   bv32Ty},
        {BINARY_IR_FUNC(CreateAShr), BINARY_EXPR_FUNC(AShr),   bv32Ty},
        {BINARY_IR_FUNC(CreateAnd), BINARY_EXPR_FUNC(BvAnd),  bv32Ty},
        {BINARY_IR_FUNC(CreateOr), BINARY_EXPR_FUNC(BvOr),   bv32Ty},
        {BINARY_IR_FUNC(CreateXor), BINARY_EXPR_FUNC(BvXor),  bv32Ty},
    };

    EXPECT_TRUE(checkBinaryOperators(tests));
}

TEST_F(InstToExprTest, TransformBinaryIntArithmeticOperator)
{
    settings.ints = IntRepresentation::Integers;

    auto& intTy = IntType::Get(context);

    TestVector<llvm::Value*(llvm::Value*, llvm::Value*), ExprPtr(ExprPtr, ExprPtr)> tests = {
        {BINARY_IR_FUNC(CreateAdd), BINARY_EXPR_FUNC(Add), intTy},
        {BINARY_IR_FUNC(CreateSub), BINARY_EXPR_FUNC(Sub), intTy},
        {BINARY_IR_FUNC(CreateMul), BINARY_EXPR_FUNC(Mul), intTy},
        // Signed and unsigned division/remainder are both over-approximated with arithmetic div/rem.
        {BINARY_IR_FUNC(CreateSDiv), BINARY_EXPR_FUNC(Div), intTy},
        {BINARY_IR_FUNC(CreateUDiv), BINARY_EXPR_FUNC(Div), intTy},
        {BINARY_IR_FUNC(CreateSRem), BINARY_EXPR_FUNC(Rem), intTy},
        {BINARY_IR_FUNC(CreateURem), BINARY_EXPR_FUNC(Rem), intTy}
    };

    EXPECT_TRUE(checkBinaryOperators(tests));
}

TEST_F(InstToExprTest, TransformBinaryLogicOperator)
{
    auto& boolTy = BoolType::Get(context);

    auto expr = transformWithImpl(ir->CreateAnd(i1Val("b1", boolTy), i1Val("b2", boolTy)), boolTy);
    EXPECT_EQ(expr, builder->And(variableFor("b1")->getRefExpr(), variableFor("b2")->getRefExpr()));

    expr = transformWithImpl(ir->CreateOr(i1Val("b3", boolTy), i1Val("b4", boolTy)), boolTy);
    EXPECT_EQ(expr, builder->Or(variableFor("b3")->getRefExpr(), variableFor("b4")->getRefExpr()));

    expr = transformWithImpl(ir->CreateXor(i1Val("b5", boolTy), i1Val("b6", boolTy)), boolTy);
    EXPECT_EQ(expr, builder->Xor(variableFor("b5")->getRefExpr(), variableFor("b6")->getRefExpr()));
}

#undef IR_BUILDER_FUNC
#undef BINARY_EXPR_FUNC

TEST_F(InstToExprTest, TransformBvCast)
{
    auto& bv32Ty = BvType::Get(context, 32);
    auto& bv64Ty = BvType::Get(context, 64);

    auto bvZext = transformWithImpl(
        ir->CreateZExt(i32Val("x", bv32Ty), llvm::Type::getInt64Ty(llvmContext)), bv64Ty);
    EXPECT_EQ(bvZext->getType(), bv64Ty);
    EXPECT_EQ(bvZext, builder->ZExt(variableFor("x")->getRefExpr(), bv64Ty));

    auto bvSext = transformWithImpl(
        ir->CreateSExt(i32Val("y", bv32Ty), llvm::Type::getInt64Ty(llvmContext)), bv64Ty);
    EXPECT_EQ(bvSext->getType(), bv64Ty);
    EXPECT_EQ(bvSext, builder->SExt(variableFor("y")->getRefExpr(), bv64Ty));

    auto& bv8Ty = BvType::Get(context, 8);
    auto bvTrunc = transformWithImpl(
        ir->CreateTrunc(i32Val("z", bv32Ty), llvm::Type::getInt8Ty(llvmContext)), bv8Ty);
    EXPECT_EQ(bvTrunc->getType(), bv8Ty);
    EXPECT_EQ(bvTrunc, builder->Extract(variableFor("z")->getRefExpr(), 0, 8));
}

TEST_F(InstToExprTest, TransformIntExtCast)
{
    auto& intTy = IntType::Get(context);

    auto zext = transformWithImpl(
        ir->CreateZExt(i32Val("x", intTy), llvm::Type::getInt64Ty(llvmContext)), intTy);
    EXPECT_EQ(zext->getType(), intTy);
    EXPECT_EQ(zext, variableFor("x")->getRefExpr());

    auto sext = transformWithImpl(
        ir->CreateSExt(i32Val("y", intTy), llvm::Type::getInt64Ty(llvmContext)), intTy);
    EXPECT_EQ(sext->getType(), intTy);
    EXPECT_EQ(sext, variableFor("y")->getRefExpr());
}

TEST_F(InstToExprTest, TransformIntTrunc_Semantic)
{
    auto& intTy = IntType::Get(context);

    auto xVal = i32Val("x", intTy);
    auto trunc = transformWithImpl(
        ir->CreateTrunc(xVal, llvm::Type::getInt8Ty(llvmContext)), intTy);

    EXPECT_EQ(evalInt(trunc, {{xVal, 257}}), builder->IntLit(1));
    EXPECT_EQ(evalInt(trunc, {{xVal, 8}}), builder->IntLit(8));
    EXPECT_EQ(evalInt(trunc, {{xVal, 7}}), builder->IntLit(7));
    EXPECT_EQ(evalInt(trunc, {{xVal, 192}}), builder->IntLit(-64));
}

// Comparison operators
//==------------------------------------------------------------------------==//
TEST_F(InstToExprTest, TransformBvCmp)
{
    auto& boolTy = BoolType::Get(context);
    auto& bv32Ty = BvType::Get(context, 32);

#define CHECK_BV_COMPARE(PREDICATE, KIND)                                                          \
{                                                                                              \
    auto theInst = transformWithImpl(                                                          \
        ir->CreateICmp(CmpInst::PREDICATE, i32Val("x", bv32Ty), i32Val("y", bv32Ty)), boolTy); \
    EXPECT_EQ(                                                                                 \
        theInst,                                                                               \
        builder->KIND(variableFor("x")->getRefExpr(), variableFor("y")->getRefExpr()));        \
}

    CHECK_BV_COMPARE(ICMP_EQ, Eq)
    CHECK_BV_COMPARE(ICMP_NE, NotEq)
    CHECK_BV_COMPARE(ICMP_SGT, BvSGt)
    CHECK_BV_COMPARE(ICMP_SGE, BvSGtEq)
    CHECK_BV_COMPARE(ICMP_SLT, BvSLt)
    CHECK_BV_COMPARE(ICMP_SLE, BvSLtEq)
    CHECK_BV_COMPARE(ICMP_UGT, BvUGt)
    CHECK_BV_COMPARE(ICMP_UGE, BvUGtEq)
    CHECK_BV_COMPARE(ICMP_ULT, BvULt)
    CHECK_BV_COMPARE(ICMP_ULE, BvULtEq)

#undef CHECK_BV_COMPARE
}

TEST_F(InstToExprTest, TransformIntCmp)
{
    auto& boolTy = BoolType::Get(context);
    auto& intTy = IntType::Get(context);

#define CHECK_INT_COMPARE(PREDICATE, KIND)                                                         \
    {                                                                                              \
        auto theInst = transformWithImpl(                                                          \
            ir->CreateICmp(CmpInst::PREDICATE, i32Val("x", intTy), i32Val("y", intTy)), boolTy);   \
        EXPECT_EQ(                                                                                 \
            theInst,                                                                               \
            builder->KIND(variableFor("x")->getRefExpr(), variableFor("y")->getRefExpr()));        \
    }

    CHECK_INT_COMPARE(ICMP_EQ,  Eq)
    CHECK_INT_COMPARE(ICMP_NE,  NotEq)
    CHECK_INT_COMPARE(ICMP_SGT, Gt)
    CHECK_INT_COMPARE(ICMP_SGE, GtEq)
    CHECK_INT_COMPARE(ICMP_SLT, Lt)
    CHECK_INT_COMPARE(ICMP_SLE, LtEq)

#undef CHECK_INT_COMPARE
}

TEST_F(InstToExprTest, TransformIntCmp_Semantic)
{
    settings.ints = IntRepresentation::Integers;

    auto lhs  = i32Val("lhs", IntType::Get(context));
    auto rhs = i32Val("rhs", IntType::Get(context));

    // 1  u> 0  => 00..01 > 00..00 => True
    // 1  u> -1 => 00..01 > 11..11 => False
    // -1 u> 1  => 11..11 > 00..01 => True
    auto ugt = transformWithImpl(ir->CreateICmpUGT(lhs, rhs), BoolType::Get(context));
    EXPECT_EQ(evalInt(ugt, {{lhs, 1},  {rhs, 1}}),  builder->False());
    EXPECT_EQ(evalInt(ugt, {{lhs, 0},  {rhs, 1}}),  builder->False());
    EXPECT_EQ(evalInt(ugt, {{lhs, 0},  {rhs, -1}}), builder->False());
    EXPECT_EQ(evalInt(ugt, {{lhs, -1}, {rhs, 1}}),  builder->True());
    EXPECT_EQ(evalInt(ugt, {{lhs, -1}, {rhs, 0}}),  builder->True());
    EXPECT_EQ(evalInt(ugt, {{lhs, 1},  {rhs, 0}}),  builder->True());
    EXPECT_EQ(evalInt(ugt, {{lhs, 1},  {rhs, -1}}), builder->False());

    auto uge = transformWithImpl(ir->CreateICmpUGE(lhs, rhs), BoolType::Get(context));
    EXPECT_EQ(evalInt(uge, {{lhs, 1},  {rhs, 1}}),  builder->True());
    EXPECT_EQ(evalInt(uge, {{lhs, 0},  {rhs, 1}}),  builder->False());
    EXPECT_EQ(evalInt(uge, {{lhs, 0},  {rhs, -1}}), builder->False());
    EXPECT_EQ(evalInt(uge, {{lhs, -1}, {rhs, 1}}),  builder->True());
    EXPECT_EQ(evalInt(uge, {{lhs, -1}, {rhs, 0}}),  builder->True());
    EXPECT_EQ(evalInt(uge, {{lhs, 1},  {rhs, 0}}),  builder->True());
    EXPECT_EQ(evalInt(uge, {{lhs, 1},  {rhs, -1}}), builder->False());

    auto ult = transformWithImpl(ir->CreateICmpULT(lhs, rhs), BoolType::Get(context));
    EXPECT_EQ(evalInt(ult, {{lhs, 1},  {rhs, 1}}),  builder->False());
    EXPECT_EQ(evalInt(ult, {{lhs, 0},  {rhs, 1}}),  builder->True());
    EXPECT_EQ(evalInt(ult, {{lhs, 0},  {rhs, -1}}), builder->True());
    EXPECT_EQ(evalInt(ult, {{lhs, -1}, {rhs, 1}}),  builder->False());
    EXPECT_EQ(evalInt(ult, {{lhs, -1}, {rhs, 0}}),  builder->False());
    EXPECT_EQ(evalInt(ult, {{lhs, 1},  {rhs, 0}}),  builder->False());
    EXPECT_EQ(evalInt(ult, {{lhs, 1},  {rhs, -1}}), builder->True());

    auto ule = transformWithImpl(ir->CreateICmpULE(lhs, rhs), BoolType::Get(context));
    EXPECT_EQ(evalInt(ule, {{lhs, 1},  {rhs, 1}}),  builder->True());
    EXPECT_EQ(evalInt(ule, {{lhs, 0},  {rhs, 1}}),  builder->True());
    EXPECT_EQ(evalInt(ule, {{lhs, 0},  {rhs, -1}}), builder->True());
    EXPECT_EQ(evalInt(ule, {{lhs, -1}, {rhs, 1}}),  builder->False());
    EXPECT_EQ(evalInt(ule, {{lhs, -1}, {rhs, 0}}),  builder->False());
    EXPECT_EQ(evalInt(ule, {{lhs, 1},  {rhs, 0}}),  builder->False());
    EXPECT_EQ(evalInt(ule, {{lhs, 1},  {rhs, -1}}), builder->True());
}

} // end anonymous namespace