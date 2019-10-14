#include "gazer/LLVM/Automaton/InstToExpr.h"

#include <llvm/IR/IRBuilder.h>

#include <gtest/gtest.h>

namespace gazer::unittest
{

class InstToExprTest : public ::testing::Test
{
public:
    struct InstVarPair
    {
        llvm::Instruction* Inst;
        Variable* Var;
    };

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

protected:
    InstToExprTest(std::unique_ptr<MemoryModel> memoryModel)
        : mBuilder(CreateExprBuilder()),
        mMemoryModel(std::move(memoryModel))
    {}

public:
    void SetUp() override
    {
        module.reset(new llvm::Module("InstToExprModule", llvmContext));
        auto funcTy = llvm::FunctionType::get(
            llvm::Type::getVoidTy(llvmContext),
            /*isVarArg=*/false
        );
        function = llvm::Function::Create(funcTy, llvm::Function::ExternalLinkage, "", module.get());
        startBB = llvm::BasicBlock::Create(llvmContext, "", function);

        ir.reset(new llvm::IRBuilder<>(startBB));
    }

    void TearDown() override
    {
        module.reset();
        function = nullptr;
        startBB = nullptr;
    }

    InstVarPair v(std::string name, llvm::Instruction* inst)
    {
        return { inst, context.createVariable(name, mMemoryModel->translateType(inst->getType())) };
    }

    std::unique_ptr<InstToExprImpl> createImpl(llvm::DenseMap<llvm::Value*, Variable*> vars)
    {
        return std::make_unique<InstToExprImpl>(*mBuilder, *mMemoryModel, settings, std::move(vars));
    }
protected:
    GazerContext context;
    llvm::LLVMContext llvmContext;
    LLVMFrontendSettings settings;
    std::unique_ptr<ExprBuilder> mBuilder;
    std::unique_ptr<MemoryModel> mMemoryModel;

    std::unique_ptr<llvm::Module> module;
    llvm::Function* function;
    llvm::BasicBlock* startBB;
    std::unique_ptr<llvm::IRBuilder<>> ir;
};

}