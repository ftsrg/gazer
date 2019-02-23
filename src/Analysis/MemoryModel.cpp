#include "gazer/Analysis/MemoryModel.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Variable.h"
#include "gazer/LLVM/Ir2Expr.h"

#include <llvm/IR/Operator.h>
#include <llvm/IR/CallSite.h>
#include <llvm/Analysis/PtrUseVisitor.h>
#include <llvm/IR/InstIterator.h>

namespace gazer
{

class HavocMemoryModel : public MemoryModel
{
public:
    using MemoryModel::MemoryModel;

    void initialize(llvm::Function& function, ValueToVariableMap& vmap) override
    {
    }

    ExprRef<> handleAlloca(llvm::AllocaInst& alloc) override {
        return BoolLiteralExpr::True(mContext);
    }

    ExprRef<> handleStore(llvm::StoreInst& store) override {
        return BoolLiteralExpr::True(mContext);
    }

    ExprRef<> handleLoad(llvm::LoadInst& load) override {
        return BoolLiteralExpr::True(mContext);
    }

    ExprRef<> handleCall(llvm::CallInst& call) override {
        return BoolLiteralExpr::True(mContext);
    }

    ExprRef<> handleGetElementPtr(llvm::GetElementPtrInst& gep) override {
        return BoolLiteralExpr::True(mContext);
    }

    ExprRef<> handlePointerCast(llvm::CastInst& cast, ExprRef<> operand) override {
        return BoolLiteralExpr::True(mContext);
    }

    ExprRef<> handlePointerValue(llvm::Value* ptr) override {
        return BoolLiteralExpr::True(mContext);
    }

    Type& getTypeFromPointerType(const llvm::PointerType* type) override {
        // Return just some bogus type.
        return BvType::Get(mContext, 1);
    }
private:
    llvm::DenseMap<llvm::Instruction*, Variable*> mVariables;
};

}

using namespace gazer;
using llvm::dyn_cast;
using llvm::cast;

std::unique_ptr<MemoryModel> gazer::createHavocMemoryModel(GazerContext& context) {
    return std::make_unique<HavocMemoryModel>(context);
}

