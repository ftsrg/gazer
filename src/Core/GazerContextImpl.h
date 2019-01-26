#ifndef _GAZER_SRC_GAZERCONTEXTIMPL_H
#define _GAZER_SRC_GAZERCONTEXTIMPL_H

#include "gazer/Core/GazerContext.h"
#include "gazer/Core/Type.h"
#include "gazer/Core/Variable.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/Support/DenseMapKeyInfo.h"

#include <llvm/ADT/DenseMap.h>

#include <unordered_set>
#include <unordered_map>

namespace gazer
{

class GazerContextImpl
{
    friend class GazerContext;

    GazerContextImpl(GazerContext& ctx);
public:
    //---------------------- Types ----------------------//
    BoolType BoolTy;
    IntType IntTy;
    BvType Bv1Ty, Bv8Ty, Bv16Ty, Bv32Ty, Bv64Ty;
    std::unordered_map<unsigned, std::unique_ptr<BvType>> BvTypes;
    FloatType FpHalfTy, FpSingleTy, FpDoubleTy, FpQuadTy;

    //-------------------- Variables --------------------//
    // This map contains all variables created by this context, with FQNs as the map key.
    llvm::StringMap<std::unique_ptr<Variable>> VariableTable;

    //------------------- Expressions -------------------//
    ExprRef<BoolLiteralExpr> TrueLit, FalseLit;
    llvm::DenseMap<llvm::APInt, ExprRef<BvLiteralExpr>, DenseMapAPIntKeyInfo> BvLiterals;
    ExprRef<BvLiteralExpr> Bv1True, Bv1False;
    llvm::DenseMap<llvm::APFloat, ExprRef<FloatLiteralExpr>, DenseMapAPFloatKeyInfo> FloatLiterals;
    llvm::DenseMap<const Type*, ExprRef<UndefExpr>> Undefs;

    // This set will contain all expressions created by this context.
    //std::unordered_set<ExprRef<>> Exprs;
};

} // end namespace gazer

#endif