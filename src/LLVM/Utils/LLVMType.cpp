#include "gazer/LLVM/Utils/LLVMType.h"

using namespace gazer;

llvm::Type* gazer::llvmTypeFromType(llvm::LLVMContext& context, const gazer::Type& type)
{
    switch (type.getTypeID()) {
        case Type::BoolTypeID:
            return llvm::Type::getInt1Ty(context);
        case Type::IntTypeID:
            return llvm::Type::getIntNTy(
                context,
                llvm::cast<IntType>(&type)->getWidth());
        case Type::FloatTypeID: {
            auto fltTy = llvm::cast<FloatType>(&type);
            switch (fltTy->getPrecision()) {
                case FloatType::Half:
                    return llvm::Type::getHalfTy(context);
                case FloatType::Single:
                    return llvm::Type::getFloatTy(context);
                case FloatType::Double:
                    return llvm::Type::getDoubleTy(context);
                case FloatType::Quad:
                    return llvm::Type::getFP128Ty(context);
            }
            llvm_unreachable("Unhandled float precision");
        }
    }

    llvm_unreachable("Unknown type.");
}
