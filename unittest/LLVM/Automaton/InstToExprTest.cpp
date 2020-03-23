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

#include <llvm/IR/IRBuilder.h>

#include <gtest/gtest.h>

using namespace gazer;
using namespace llvm;

namespace {

class InstToExprTest : public ::testing::Test
{
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
    llvm::GlobalVariable *gv1, *gv2, *gv3;
    llvm::GlobalVariable *b1, *b2, *b3;
    std::unique_ptr<llvm::IRBuilder<>> ir;

public:
    InstToExprTest() : builder(CreateExprBuilder(context)) {}

    void SetUp() override
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

        gv1 = new GlobalVariable(
            *module, llvm::Type::getInt32Ty(llvmContext), /*isConstant=*/false,
            GlobalValue::ExternalLinkage, nullptr);
        gv2 = new GlobalVariable(
            *module, llvm::Type::getInt32Ty(llvmContext), /*isConstant=*/false,
            GlobalValue::ExternalLinkage, nullptr);
        gv3 = new GlobalVariable(
            *module, llvm::Type::getInt32Ty(llvmContext), /*isConstant=*/false,
            GlobalValue::ExternalLinkage, nullptr);
        b1 = new GlobalVariable(
            *module, llvm::Type::getInt1Ty(llvmContext), /*isConstant=*/false,
            GlobalValue::ExternalLinkage, nullptr);
        b2 = new GlobalVariable(
            *module, llvm::Type::getInt1Ty(llvmContext), /*isConstant=*/false,
            GlobalValue::ExternalLinkage, nullptr);
        b3 = new GlobalVariable(
            *module, llvm::Type::getInt1Ty(llvmContext), /*isConstant=*/false,
            GlobalValue::ExternalLinkage, nullptr);
    }

    void TearDown() override
    {
        module.reset();
        function = nullptr;
        startBB = nullptr;
    }

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

    std::unique_ptr<InstToExprImpl> createImpl(llvm::DenseMap<llvm::Value*, Variable*> vars)
    {
        return std::make_unique<InstToExprImpl>(
            *function, *builder, *types, memoryModel->getMemoryInstructionHandler(*function),
            settings, std::move(vars));
    }

    // Test helpers
    //==--------------------------------------------------------------------==//
public:
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

template<class IrBuilderFuncTy, class ExprBuilderFuncTy>
auto InstToExprTest::checkBinaryOperators(const TestVector<IrBuilderFuncTy, ExprBuilderFuncTy>& tests)
    -> ::testing::AssertionResult
{
    auto loadGv1 = ir->CreateLoad(gv1->getValueType(), gv1);
    auto loadGv2 = ir->CreateLoad(gv2->getValueType(), gv2);

    for (unsigned i = 0; i < tests.size(); ++i) {
        auto& triple = tests[i];

        auto gv1Var = context.createVariable("gv1_" + std::to_string(i), triple.expectedType);
        auto gv2Var = context.createVariable("gv2_" + std::to_string(i), triple.expectedType);
        auto instVar =
            context.createVariable("inst_" + std::to_string(i), BvType::Get(context, 32));

        llvm::Value* inst = triple.irBuilder(loadGv1, loadGv2);
        ExprPtr expected = triple.exprBuilder(gv1Var->getRefExpr(), gv2Var->getRefExpr());

        auto inst2expr = createImpl({{loadGv1, gv1Var}, {loadGv2, gv2Var}, {inst, instVar}});
        auto actual = inst2expr->transform(*cast<Instruction>(inst), triple.expectedType);

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
    settings.ints = IntRepresentation::BitVectors;

    auto loadB1 = ir->CreateLoad(b1->getValueType(), b1);
    auto loadB2 = ir->CreateLoad(b2->getValueType(), b2);

    auto b1Var = context.createVariable("load_gv1", BoolType::Get(context));
    auto b2Var = context.createVariable("load_gv2", BoolType::Get(context));

    auto inst2expr = createImpl({{loadB1, b1Var}, {loadB2, b2Var}});

    EXPECT_EQ(
        inst2expr->transform(
            *cast<Instruction>(ir->CreateAnd(loadB1, loadB2)), BoolType::Get(context)),
        builder->And(b1Var->getRefExpr(), b2Var->getRefExpr()));
    EXPECT_EQ(
        inst2expr->transform(
            *cast<Instruction>(ir->CreateOr(loadB1, loadB2)), BoolType::Get(context)),
        builder->Or(b1Var->getRefExpr(), b2Var->getRefExpr()));
    EXPECT_EQ(
        inst2expr->transform(
            *cast<Instruction>(ir->CreateXor(loadB1, loadB2)), BoolType::Get(context)),
        builder->NotEq(b1Var->getRefExpr(), b2Var->getRefExpr()));
}

#undef IR_BUILDER_FUNC
#undef BINARY_EXPR_FUNC


TEST_F(InstToExprTest, TransformBvCast)
{
    settings.ints = IntRepresentation::BitVectors;

    auto loadGv1 = ir->CreateLoad(gv1->getValueType(), gv1);
    auto loadGv2 = ir->CreateLoad(gv2->getValueType(), gv2);
    auto loadGv3 = ir->CreateLoad(gv3->getValueType(), gv3);
    auto zext = ir->CreateZExt(loadGv1, llvm::IntegerType::getInt64Ty(llvmContext));
    auto sext = ir->CreateSExt(loadGv2, llvm::IntegerType::getInt64Ty(llvmContext));
    auto trunc = ir->CreateTrunc(loadGv3, llvm::IntegerType::getInt8Ty(llvmContext));

    auto gv1Var = context.createVariable("load_gv1", BvType::Get(context, 32));
    auto gv2Var = context.createVariable("load_gv2", BvType::Get(context, 32));
    auto gv3Var = context.createVariable("load_gv3", BvType::Get(context, 32));
    auto zextVar = context.createVariable("zext", BvType::Get(context, 64));
    auto sextVar = context.createVariable("sext", BvType::Get(context, 64));
    auto truncVar = context.createVariable("trunc", BvType::Get(context, 8));

    auto inst2expr = createImpl({{loadGv1, gv1Var},
                                 {loadGv2, gv2Var},
                                 {loadGv3, gv3Var},
                                 {zext, zextVar},
                                 {sext, sextVar},
                                 {trunc, truncVar}});

    ASSERT_EQ(
        inst2expr->transform(*cast<Instruction>(zext), BvType::Get(context, 64)),
        builder->ZExt(gv1Var->getRefExpr(), BvType::Get(context, 64)));
    ASSERT_EQ(
        inst2expr->transform(*cast<Instruction>(sext), BvType::Get(context, 64)),
        builder->SExt(gv2Var->getRefExpr(), BvType::Get(context, 64)));
    ASSERT_EQ(
        inst2expr->transform(*cast<Instruction>(trunc), BvType::Get(context, 8)),
        builder->Extract(gv3Var->getRefExpr(), 0, 8));
}

// A little helper macro to help us create and translate comparisons.
#define TRANSLATE_COMPARE(PRED)                                                                    \
    (inst2expr->transform(*cast<Instruction>(ir->CreateICmp(CmpInst::PRED, loadGv1, loadGv2))))

#define CHECK_COMPARE(PREDICATE, KIND)                                                             \
    EXPECT_EQ(                                                                                     \
        inst2expr->transform(                                                                      \
            *cast<Instruction>(ir->CreateICmp(CmpInst::PREDICATE, loadGv1, loadGv2)),              \
            BoolType::Get(context)),                                                               \
        builder->KIND(gv1Var->getRefExpr(), gv2Var->getRefExpr()))

TEST_F(InstToExprTest, TransformBvCmp)
{
    settings.ints = IntRepresentation::BitVectors;

    auto loadGv1 = ir->CreateLoad(gv1->getValueType(), gv1);
    auto loadGv2 = ir->CreateLoad(gv2->getValueType(), gv2);

    auto gv1Var = context.createVariable("load_gv1", BvType::Get(context, 32));
    auto gv2Var = context.createVariable("load_gv2", BvType::Get(context, 32));

    auto inst2expr = createImpl({{loadGv1, gv1Var}, {loadGv2, gv2Var}});

    CHECK_COMPARE(ICMP_EQ, Eq);
    CHECK_COMPARE(ICMP_NE, NotEq);
    CHECK_COMPARE(ICMP_SGT, BvSGt);
    CHECK_COMPARE(ICMP_SGE, BvSGtEq);
    CHECK_COMPARE(ICMP_SLT, BvSLt);
    CHECK_COMPARE(ICMP_SLE, BvSLtEq);
    CHECK_COMPARE(ICMP_UGT, BvUGt);
    CHECK_COMPARE(ICMP_UGE, BvUGtEq);
    CHECK_COMPARE(ICMP_ULT, BvULt);
    CHECK_COMPARE(ICMP_ULE, BvULtEq);
}

TEST_F(InstToExprTest, TransformIntCmp)
{
    settings.ints = IntRepresentation::Integers;

    auto loadGv1 = ir->CreateLoad(gv1->getValueType(), gv1);
    auto loadGv2 = ir->CreateLoad(gv2->getValueType(), gv2);

    auto gv1Var = context.createVariable("load_gv1", IntType::Get(context));
    auto gv2Var = context.createVariable("load_gv2", IntType::Get(context));

    auto inst2expr = createImpl({{loadGv1, gv1Var}, {loadGv2, gv2Var}});

    CHECK_COMPARE(ICMP_EQ, Eq);
    CHECK_COMPARE(ICMP_NE, NotEq);
    CHECK_COMPARE(ICMP_SGT, Gt);
    CHECK_COMPARE(ICMP_SGE, GtEq);
    CHECK_COMPARE(ICMP_SLT, Lt);
    CHECK_COMPARE(ICMP_SLE, LtEq);

    // TODO: Unordered comparisons require special assertions.
}

#undef TRANSLATE_COMPARE

} // end anonymous namespace