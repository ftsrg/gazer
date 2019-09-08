#include "gazer/LLVM/Automaton/InstToExpr.h"

#include <llvm/IR/IRBuilder.h>

#include <gtest/gtest.h>

using namespace gazer;
using namespace llvm;

namespace
{

class InstToExprTest : public ::testing::Test
{
public:
    GazerContext context;
    llvm::LLVMContext llvmContext;
    LLVMFrontendSettings settings;
    std::unique_ptr<ExprBuilder> builder;
    std::unique_ptr<MemoryModel> memoryModel;

    std::unique_ptr<llvm::Module> module;
    llvm::Function* function;
    llvm::BasicBlock* startBB;
    llvm::GlobalVariable *gv1, *gv2, *gv3;
    llvm::GlobalVariable *b1, *b2, *b3;
    std::unique_ptr<llvm::IRBuilder<>> ir;
public:
    InstToExprTest()
        : builder(CreateExprBuilder(context)),
        memoryModel(new DummyMemoryModel(context, settings))
    {}

    void SetUp() override
    {
        module.reset(new llvm::Module("InstToExprModule", llvmContext));
        auto funcTy = llvm::FunctionType::get(
            llvm::Type::getVoidTy(llvmContext),
            /*isVarArg=*/false
        );
        function = Function::Create(funcTy, Function::ExternalLinkage, "", module.get());
        startBB = BasicBlock::Create(llvmContext, "", function);

        ir.reset(new IRBuilder<>(startBB));

        gv1 = new GlobalVariable(
            *module, llvm::Type::getInt32Ty(llvmContext), /*isConstant=*/false, GlobalValue::ExternalLinkage, nullptr
        );
        gv2 = new GlobalVariable(
            *module, llvm::Type::getInt32Ty(llvmContext), /*isConstant=*/false, GlobalValue::ExternalLinkage, nullptr
        );
        gv3 = new GlobalVariable(
            *module, llvm::Type::getInt32Ty(llvmContext), /*isConstant=*/false, GlobalValue::ExternalLinkage, nullptr
        );
        b1 = new GlobalVariable(
            *module, llvm::Type::getInt1Ty(llvmContext), /*isConstant=*/false, GlobalValue::ExternalLinkage, nullptr
        );
        b2 = new GlobalVariable(
            *module, llvm::Type::getInt1Ty(llvmContext), /*isConstant=*/false, GlobalValue::ExternalLinkage, nullptr
        );
        b3 = new GlobalVariable(
            *module, llvm::Type::getInt1Ty(llvmContext), /*isConstant=*/false, GlobalValue::ExternalLinkage, nullptr
        );
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
            ExprBuilder& builder,
            MemoryModel& memoryModel,
            LLVMFrontendSettings settings,
            llvm::DenseMap<llvm::Value*, Variable*> vars
        ) : InstToExpr(builder, memoryModel, settings), mVars(std::move(vars))
        {}

        Variable* getVariable(const llvm::Value* value) override {
            return mVars.lookup(value);
        }

    private:
        llvm::DenseMap<llvm::Value*, Variable*> mVars;
    };

    std::unique_ptr<InstToExprImpl> createImpl(llvm::DenseMap<llvm::Value*, Variable*> vars)
    {
        return std::make_unique<InstToExprImpl>(*builder, *memoryModel, settings, std::move(vars));
    }
};

TEST_F(InstToExprTest, TransformBinaryArithmeticOperator)
{
    auto loadGv1 = ir->CreateLoad(gv1->getValueType(), gv1);
    auto add = ir->CreateAdd(loadGv1, ir->getInt32(128));
    auto sub = ir->CreateSub(add, ir->getInt32(1));
    auto loadGv2 = ir->CreateLoad(gv2->getValueType(), gv2);
    auto mul = ir->CreateMul(sub, loadGv2);

    auto gv1Var = context.createVariable("load_gv1", BvType::Get(context, 32));
    auto gv2Var = context.createVariable("load_gv2", BvType::Get(context, 32));
    auto addVar = context.createVariable("add", BvType::Get(context, 32));
    auto subVar = context.createVariable("sub", BvType::Get(context, 32));
    auto mulVar = context.createVariable("mul", BvType::Get(context, 32));

    auto inst2expr = createImpl({
        { loadGv1,  gv1Var },
        { loadGv2,  gv2Var },
        { add,      addVar },
        { sub,      subVar },
        { mul,      mulVar }
    });

    ASSERT_EQ(
        inst2expr->transform(*cast<Instruction>(add)),
        builder->Add(gv1Var->getRefExpr(), builder->BvLit32(128))
    );
    ASSERT_EQ(
        inst2expr->transform(*cast<Instruction>(sub)),
        builder->Sub(addVar->getRefExpr(), builder->BvLit32(1))
    );
    ASSERT_EQ(
        inst2expr->transform(*cast<Instruction>(mul)),
        builder->Mul(subVar->getRefExpr(), gv2Var->getRefExpr())
    );
}

TEST_F(InstToExprTest, TransformBinaryLogicOperator)
{
    auto loadB1 = ir->CreateLoad(b1->getValueType(), b1);
    auto loadB2 = ir->CreateLoad(b2->getValueType(), b2);
    auto loadB3 = ir->CreateLoad(b3->getValueType(), b3);

    auto b1Var = context.createVariable("load_gv1", BoolType::Get(context));
    auto b2Var = context.createVariable("load_gv2", BoolType::Get(context));
    auto b3Var = context.createVariable("load_gv3", BoolType::Get(context));

    auto inst2expr = createImpl({
        { loadB1, b1Var },
        { loadB2, b2Var },
        { loadB3, b3Var }
    });

    EXPECT_EQ(
        inst2expr->transform(*cast<Instruction>(
            ir->CreateAnd(loadB1, loadB2)
        )),
        builder->And(b1Var->getRefExpr(), b2Var->getRefExpr())
    );
}

TEST_F(InstToExprTest, TransformBvCast)
{
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

    auto inst2expr = createImpl({
        { loadGv1, gv1Var },
        { loadGv2, gv2Var },
        { loadGv3, gv3Var },
        { zext, zextVar },
        { sext, sextVar },
        { trunc, truncVar }
    });

    ASSERT_EQ(
        inst2expr->transform(*cast<Instruction>(zext)),
        builder->ZExt(gv1Var->getRefExpr(), BvType::Get(context, 64))
    );
    ASSERT_EQ(
        inst2expr->transform(*cast<Instruction>(sext)),
        builder->SExt(gv2Var->getRefExpr(), BvType::Get(context, 64))
    );
    ASSERT_EQ(
        inst2expr->transform(*cast<Instruction>(trunc)),
        builder->Extract(gv3Var->getRefExpr(), 0, 8)
    );
}


TEST_F(InstToExprTest, TransformBvCmp)
{
    auto loadGv1 = ir->CreateLoad(gv1->getValueType(), gv1);
    auto loadGv2 = ir->CreateLoad(gv2->getValueType(), gv2);
    auto loadGv3 = ir->CreateLoad(gv3->getValueType(), gv3);

    auto gv1Var = context.createVariable("load_gv1", BvType::Get(context, 32));
    auto gv2Var = context.createVariable("load_gv2", BvType::Get(context, 32));
    auto gv3Var = context.createVariable("load_gv3", BvType::Get(context, 32));

    auto inst2expr = createImpl({
        { loadGv1, gv1Var },
        { loadGv2, gv2Var },
        { loadGv3, gv3Var },
    });

    EXPECT_EQ(
        inst2expr->transform(*cast<Instruction>(
            ir->CreateICmpEQ(loadGv1, loadGv2)
        )),
        builder->Eq(gv1Var->getRefExpr(), gv2Var->getRefExpr())
    );
    EXPECT_EQ(
        inst2expr->transform(*cast<Instruction>(
            ir->CreateICmpNE(loadGv1, loadGv2)
        )),
        builder->NotEq(gv1Var->getRefExpr(), gv2Var->getRefExpr())
    );
    EXPECT_EQ(
        inst2expr->transform(*cast<Instruction>(
            ir->CreateICmpUGT(loadGv1, loadGv2)
        )),
        builder->BvUGt(gv1Var->getRefExpr(), gv2Var->getRefExpr())
    );
    EXPECT_EQ(
        inst2expr->transform(*cast<Instruction>(
            ir->CreateICmpUGE(loadGv1, loadGv2)
        )),
        builder->BvUGtEq(gv1Var->getRefExpr(), gv2Var->getRefExpr())
    );
    EXPECT_EQ(
        inst2expr->transform(*cast<Instruction>(
            ir->CreateICmpULT(loadGv1, loadGv2)
        )),
        builder->BvULt(gv1Var->getRefExpr(), gv2Var->getRefExpr())
    );
    EXPECT_EQ(
        inst2expr->transform(*cast<Instruction>(
            ir->CreateICmpULE(loadGv1, loadGv2)
        )),
        builder->BvULtEq(gv1Var->getRefExpr(), gv2Var->getRefExpr())
    );
    EXPECT_EQ(
        inst2expr->transform(*cast<Instruction>(
            ir->CreateICmpSGT(loadGv1, loadGv2)
        )),
        builder->BvSGt(gv1Var->getRefExpr(), gv2Var->getRefExpr())
    );
    EXPECT_EQ(
        inst2expr->transform(*cast<Instruction>(
            ir->CreateICmpSGE(loadGv1, loadGv2)
        )),
        builder->BvSGtEq(gv1Var->getRefExpr(), gv2Var->getRefExpr())
    );
    EXPECT_EQ(
        inst2expr->transform(*cast<Instruction>(
            ir->CreateICmpSLT(loadGv1, loadGv2)
        )),
        builder->BvSLt(gv1Var->getRefExpr(), gv2Var->getRefExpr())
    );
    EXPECT_EQ(
        inst2expr->transform(*cast<Instruction>(
            ir->CreateICmpSLE(loadGv1, loadGv2)
        )),
        builder->BvSLtEq(gv1Var->getRefExpr(), gv2Var->getRefExpr())
    );
}


} // end anonymous namespace