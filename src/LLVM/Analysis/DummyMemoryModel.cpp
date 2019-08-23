#include "gazer/LLVM/Analysis/MemoryObject.h"
#include "gazer/Core/LiteralExpr.h"

#include "gazer/LLVM/TypeTranslator.h"

using namespace gazer;

void DummyMemoryModel::findMemoryObjects(
    llvm::Function& function,
    std::vector<MemoryObject*>& objects
) {
}

ExprRef<> DummyMemoryModel::handleLoad(const llvm::LoadInst& load)
{
    return UndefExpr::Get(mTypes.get(load.getType()));
}

ExprRef<> DummyMemoryModel::handleGetElementPtr(const llvm::GEPOperator& gep)
{
    return UndefExpr::Get(BvType::Get(mContext, 32));
}

std::optional<VariableAssignment> DummyMemoryModel::handleStore(
    const llvm::StoreInst& store, ExprPtr pointer, ExprPtr value
) {
    return std::nullopt;
}

gazer::Type& DummyMemoryModel::handlePointerType(const llvm::PointerType* type)
{
    return BvType::Get(mContext, 32);
}

gazer::Type& DummyMemoryModel::handleArrayType(const llvm::ArrayType* type)
{
    return ArrayType::Get(mContext, BvType::Get(mContext, 32), BvType::Get(mContext, 8));
}
