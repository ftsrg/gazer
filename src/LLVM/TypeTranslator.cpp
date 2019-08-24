#include "gazer/LLVM/TypeTranslator.h"

#include "gazer/LLVM/Analysis/MemoryObject.h"

using namespace gazer;

LLVMTypeTranslator::LLVMTypeTranslator(MemoryModel& memoryModel)
    : mMemoryModel(memoryModel)
{}

gazer::Type& LLVMTypeTranslator::get(const llvm::Type* type)
{
    assert(type != nullptr && "Cannot translate NULL types!");
    assert(type->isFirstClassType() && "Can only translate first class types!");

    auto& ctx = mMemoryModel.getContext();

    switch (type->getTypeID()) {
        case llvm::Type::IntegerTyID: {
            auto width = type->getIntegerBitWidth();
            if (width == 1) {
                return BoolType::Get(ctx);
            }


            return BvType::Get(ctx, width);
        }
        case llvm::Type::HalfTyID:
            return FloatType::Get(ctx, FloatType::Half);
        case llvm::Type::FloatTyID:
            return FloatType::Get(ctx, FloatType::Single);
        case llvm::Type::DoubleTyID:
            return FloatType::Get(ctx, FloatType::Double);
        case llvm::Type::FP128TyID:
            return FloatType::Get(ctx, FloatType::Quad);
        case llvm::Type::PointerTyID:
            return mMemoryModel.handlePointerType(llvm::cast<llvm::PointerType>(type));
        case llvm::Type::ArrayTyID:
            return mMemoryModel.handleArrayType(llvm::cast<llvm::ArrayType>(type));
        default:
            llvm::errs() << "Unsupported LLVM Type: " << *type << "\n";
            llvm_unreachable("Unsupported LLVM type.");
    }
}