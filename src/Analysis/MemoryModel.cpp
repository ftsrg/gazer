#include "gazer/Analysis/LegacyMemoryModel.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Variable.h"
#include "gazer/LLVM/Ir2Expr.h"

#include <llvm/IR/Operator.h>
#include <llvm/IR/CallSite.h>
#include <llvm/Analysis/PtrUseVisitor.h>
#include <llvm/IR/InstIterator.h>

namespace gazer
{



namespace legacy {

/// A dummy memory model which basically does nothing and havoc's all loads.
class HavocMemoryModel : public MemoryModel
{
public:
    using MemoryModel::MemoryModel;

    void initialize(llvm::Function& function, ValueToVariableMap& vmap) override
    {
        // Add global variables
        for (llvm::GlobalVariable& gv : function.getParent()->globals()) {
            Variable* variable = mContext.createVariable(
                gv.getName(),
                this->getTypeFromPointerType(gv.getType())
            );
            vmap[&gv] = variable;
        }
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

    ExprRef<> handleGetElementPtr(llvm::GetElementPtrInst& gep, const ExprVector& operands) override {
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

    ExprRef<> getNullPointer() const override {
        return UndefExpr::Get(BvType::Get(mContext, 1));
    }
private:
    llvm::DenseMap<llvm::Instruction*, Variable*> mVariables;
};

/// A flat memory model which translates all variables on the heap into a single array.
class FlatMemoryModel : public MemoryModel
{
public:
};

}
}

using namespace gazer;
using llvm::dyn_cast;
using llvm::cast;

std::unique_ptr<gazer::legacy::MemoryModel> gazer::legacy::createHavocMemoryModel(GazerContext& context) {
    return std::make_unique<legacy::HavocMemoryModel>(context);
}

