//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
#include "Z3SolverImpl.h"

#include "gazer/Core/LiteralExpr.h"
#include "gazer/Core/Solver/Model.h"

#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/Support/raw_ostream.h>

using namespace gazer;

namespace
{

class Z3Model : public Model
{
public:
    Z3Model(GazerContext& context, Z3_context& z3Context, Z3_model model, Z3DeclMapTy& decls)
        : mContext(context), mZ3Context(z3Context), mModel(model), mDecls(decls)
    {
        Z3_model_inc_ref(mZ3Context, mModel);
    }

    ExprRef<LiteralExpr> eval(Variable& variable) override;

    void dump(llvm::raw_ostream& os) override {
        os << Z3_model_to_string(mZ3Context, mModel);
    }

    ~Z3Model() override {
        Z3_model_dec_ref(mZ3Context, mModel);
    }

private:
    ExprRef<BoolLiteralExpr> evalBoolean(Z3AstHandle ast);
    ExprRef<BvLiteralExpr> evalBv(Z3AstHandle ast, unsigned width);
    ExprRef<FloatLiteralExpr> evalFloat(Z3AstHandle ast, FloatType::FloatPrecision prec);
    ExprRef<IntLiteralExpr> evalInt(Z3AstHandle ast);

    FloatType::FloatPrecision getFloatPrecision(Z3Handle<Z3_sort> sort);

private:
    GazerContext& mContext;
    Z3_context& mZ3Context;
    Z3_model mModel;
    Z3DeclMapTy& mDecls;
};

} // end anonymous namespace

auto Z3Solver::getModel() -> std::unique_ptr<Model>
{
    return std::make_unique<Z3Model>(
        mContext, mZ3Context, Z3_solver_get_model(mZ3Context, mSolver), mDecls);
}

auto Z3Model::eval(Variable& variable) -> ExprRef<LiteralExpr>
{
    auto declOpt = mDecls.get(&variable);
    assert(declOpt && "The model can only evaluate variables which were added to the solver!");

    auto decl = *declOpt;
    if (!Z3_model_has_interp(mZ3Context, mModel, decl)) {
        llvm::errs() << "NO INTERP " << variable << "\n";
        // There is no interpretation, the value is a don't care.
        return nullptr;
    }

    auto ast = Z3AstHandle(mZ3Context, Z3_model_get_const_interp(mZ3Context, mModel, decl));
    auto sort = Z3Handle<Z3_sort>(mZ3Context, Z3_get_sort(mZ3Context, ast));

    Z3_sort_kind kind = Z3_get_sort_kind(mZ3Context, sort);

    switch (kind) {
        case Z3_BOOL_SORT:
            return this->evalBoolean(ast);
        case Z3_INT_SORT:
            return this->evalInt(ast);
        case Z3_BV_SORT:
            return this->evalBv(ast, Z3_get_bv_sort_size(mZ3Context, sort));
        case Z3_FLOATING_POINT_SORT:
            return this->evalFloat(ast, this->getFloatPrecision(sort));
    }

    llvm_unreachable("Unknown Z3 sort!");
}

auto Z3Model::evalBoolean(Z3AstHandle ast) -> ExprRef<BoolLiteralExpr> 
{
    auto result = Z3_get_bool_value(mZ3Context, ast);
    switch (result) {
        case Z3_L_TRUE:  return BoolLiteralExpr::True(mContext);
        case Z3_L_FALSE: return BoolLiteralExpr::False(mContext);
        case Z3_L_UNDEF:
        default:
            llvm_unreachable("A function of boolean sort must be convertible to a boolean value!");
            break;
    }
}

auto Z3Model::evalInt(Z3AstHandle ast) -> ExprRef<IntLiteralExpr> 
{
    int64_t intVal;
    Z3_get_numeral_int64(mZ3Context, ast, &intVal);
    return IntLiteralExpr::Get(IntType::Get(mContext), intVal);
}

auto Z3Model::evalBv(Z3AstHandle ast, unsigned width) -> ExprRef<BvLiteralExpr>
{
    llvm::APInt value(width, Z3_get_numeral_string(mZ3Context, ast), 10);
    return BvLiteralExpr::Get(BvType::Get(mContext, width), value);
}

auto Z3Model::evalFloat(Z3AstHandle ast, FloatType::FloatPrecision prec)
    -> ExprRef<FloatLiteralExpr>
{
    FloatType& fltTy = FloatType::Get(mContext, prec);
    auto& semantics = fltTy.getLLVMSemantics();

    llvm::APFloat result(semantics);
    if (Z3_fpa_is_numeral_nan(mZ3Context, ast)) {
        result = llvm::APFloat::getNaN(semantics);
    } else if (Z3_fpa_is_numeral_zero(mZ3Context, ast)) {
        bool isNegative = Z3_fpa_is_numeral_negative(mZ3Context, ast);
        result = llvm::APFloat::getZero(semantics, isNegative);
    } else if (Z3_fpa_is_numeral_inf(mZ3Context, ast)) {
        bool isNegative = Z3_fpa_is_numeral_negative(mZ3Context, ast);
        result = llvm::APFloat::getInf(semantics, isNegative);
    } else {
        llvm::APInt intVal(fltTy.getWidth(), Z3_get_numeral_string(mZ3Context, ast), 10);
        result = llvm::APFloat(semantics, intVal);
    }

    return FloatLiteralExpr::Get(fltTy, result);
}

auto Z3Model::getFloatPrecision(Z3Handle<Z3_sort> sort) -> FloatType::FloatPrecision
{
    assert(Z3_get_sort_kind(mZ3Context, sort) == Z3_sort_kind::Z3_FLOATING_POINT_SORT);

    unsigned width = Z3_fpa_get_ebits(mZ3Context, sort) + Z3_fpa_get_sbits(mZ3Context, sort);

    switch (width) {
        case 16:
            return FloatType::Half;
        case 32:
            return FloatType::Single;
        case 64:
            return FloatType::Double;
        case 128:
            return FloatType::Quad;
        default:
            llvm_unreachable("Unknown floating-point size!");
    }
}