
#include "GazerContextImpl.h"

#include <llvm/Support/Allocator.h>

#include <unordered_set>

using namespace gazer;

GazerContext::GazerContext()
    : pImpl(new GazerContextImpl(*this))
{}

GazerContext::~GazerContext() {}

Variable* GazerContext::createVariable(std::string name, Type &type)
{
    auto ptr = new Variable(name, type);
    pImpl->VariableTable[name] = std::unique_ptr<Variable>(ptr);

    return ptr;
}

Variable* GazerContext::getVariable(llvm::StringRef name)
{
    auto result = pImpl->VariableTable.find(name);
    if (result == pImpl->VariableTable.end()) {
        return nullptr;
    }

    return result->second.get();
}

GazerContextImpl::GazerContextImpl(GazerContext& ctx)
    :
    // Types
    BoolTy(ctx), IntTy(ctx),
    Bv1Ty(ctx, 1), Bv8Ty(ctx, 8), Bv16Ty(ctx, 16), Bv32Ty(ctx, 32), Bv64Ty(ctx, 64),
    FpHalfTy(ctx, FloatType::Half), FpSingleTy(ctx, FloatType::Single),
    FpDoubleTy(ctx, FloatType::Double), FpQuadTy(ctx, FloatType::Quad),
    // Expressions
    TrueLit(new BoolLiteralExpr(BoolTy, true)),
    FalseLit(new BoolLiteralExpr(BoolTy, false)),
    Bv1True(new BvLiteralExpr(Bv1Ty, llvm::APInt(1, 1))),
    Bv1False(new BvLiteralExpr(Bv1Ty, llvm::APInt(1, 0)))
{}
