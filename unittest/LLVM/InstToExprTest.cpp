#include "gazer/LLVM/InstToExpr.h"

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
    std::unique_ptr<ExprBuilder> builder;
    std::unique_ptr<MemoryModel> memoryModel;

    std::unique_ptr<llvm::Module> module;
    llvm::Function* function;
    llvm::BasicBlock* startBB;
    llvm::GlobalVariable *gv1, *gv2, *gv3;
    std::unique_ptr<llvm::IRBuilder<>> ir;
public:
    InstToExprTest()
        : builder(CreateExprBuilder(context)),
        memoryModel(new DummyMemoryModel(context))
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
            llvm::DenseMap<llvm::Value*, Variable*> vars
        ) : InstToExpr(builder, memoryModel), mVars(std::move(vars))
        {}

        Variable* getVariable(const llvm::Value* value) override {
            return mVars.lookup(value);
        }

    private:
        llvm::DenseMap<llvm::Value*, Variable*> mVars;
    };

    std::unique_ptr<InstToExprImpl> createImpl(llvm::DenseMap<llvm::Value*, Variable*> vars)
    {
        return std::make_unique<InstToExprImpl>(*builder, *memoryModel, std::move(vars));
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

} // end anonymous namespace