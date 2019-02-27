#ifndef GAZER_ANALYSIS_MEMORYMODEL_H
#define GAZER_ANALYSIS_MEMORYMODEL_H

#include "gazer/Core/Expr.h"
#include "gazer/LLVM/Ir2Expr.h"

namespace llvm {
    class Instruction;
    class LoadInst;
    class StoreInst;
    class AllocaInst;
}

namespace gazer
{

class MemoryModel
{
public:
    MemoryModel(GazerContext& context)
        : mContext(context)
    {}

    /// This method is used to perform the initialization of the memory model.
    /// This includes the declaration and insertion of necessary variables,
    /// calculation of some required analysis algorithm, etc.
    virtual void initialize(llvm::Function& function, ValueToVariableMap& vmap) = 0;

    virtual ExprRef<> handleAlloca(llvm::AllocaInst& alloc) = 0;
    virtual ExprRef<> handleStore(llvm::StoreInst& store) = 0;
    virtual ExprRef<> handleLoad(llvm::LoadInst& load) = 0;
    virtual ExprRef<> handleCall(llvm::CallInst& call) = 0;
    virtual ExprRef<> handleGetElementPtr(llvm::GetElementPtrInst& gep, const ExprVector& operands) = 0;

    virtual ExprRef<> handlePointerCast(llvm::CastInst& cast, ExprRef<> operand) = 0;
    virtual ExprRef<> handlePointerValue(llvm::Value* ptr) = 0;

    virtual Type& getTypeFromPointerType(const llvm::PointerType* type) = 0;
    virtual ExprRef<> getNullPointer() const = 0;

    virtual ~MemoryModel() = default;

protected:
    GazerContext& mContext;
};

/// Returns an instance of an extremely primitive memory model
/// which replaces definitions with no-ops and loads with havoc instructions.
std::unique_ptr<MemoryModel> createHavocMemoryModel(GazerContext& context);

}

#endif
