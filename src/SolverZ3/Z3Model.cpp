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
    Z3Model(GazerContext& context, Z3_context& z3Context, Z3_model model, Z3ExprTransformer& exprs)
        : mContext(context), mZ3Context(z3Context), mModel(model), mExprTransformer(exprs)
    {
        Z3_model_inc_ref(mZ3Context, mModel);
    }

    Z3Model(const Z3Model&) = delete;
    Z3Model& operator=(const Z3Model&) = delete;

    ExprRef<AtomicExpr> evaluate(const ExprPtr& expr) override;

    void dump(llvm::raw_ostream& os) override {
        os << Z3_model_to_string(mZ3Context, mModel);
    }

    ~Z3Model() override {
        Z3_model_dec_ref(mZ3Context, mModel);
    }

private:
    ExprRef<AtomicExpr> evalAst(Z3AstHandle ast);
    ExprRef<BoolLiteralExpr> evalBoolean(Z3AstHandle ast);
    ExprRef<BvLiteralExpr> evalBv(Z3AstHandle ast, unsigned width);
    ExprRef<FloatLiteralExpr> evalFloat(Z3AstHandle ast, FloatType::FloatPrecision prec);
    ExprRef<IntLiteralExpr> evalInt(Z3AstHandle ast);

    ExprRef<AtomicExpr> evalConstantArray(Z3AstHandle ast, ArrayType& type);

    FloatType::FloatPrecision getFloatPrecision(Z3Handle<Z3_sort> sort);
    Type& sortToType(Z3Handle<Z3_sort> sort);

private:
    GazerContext& mContext;
    Z3_context& mZ3Context;
    Z3_model mModel;
    Z3ExprTransformer& mExprTransformer;
};

} // end anonymous namespace

auto Z3Solver::getModel() -> std::unique_ptr<Model>
{
    return std::make_unique<Z3Model>(
        mContext, mZ3Context, Z3_solver_get_model(mZ3Context, mSolver), mTransformer);
}

auto Z3Model::evaluate(const ExprPtr& expr) -> ExprRef<AtomicExpr>
{
    auto ast = mExprTransformer.translate(expr);
    return this->evalAst(ast);
}

auto Z3Model::evalAst(Z3AstHandle ast) -> ExprRef<AtomicExpr>
{
    Z3_ast resultAst;

    bool success = Z3_model_eval(mZ3Context, mModel, ast, true, &resultAst);
    assert(success);

    Z3AstHandle result(mZ3Context, resultAst);
    auto sort = Z3Handle<Z3_sort>(mZ3Context, Z3_get_sort(mZ3Context, result));
    Z3_sort_kind kind = Z3_get_sort_kind(mZ3Context, sort);

    switch (kind) {
        case Z3_BOOL_SORT:
            return this->evalBoolean(result);
        case Z3_INT_SORT:
            return this->evalInt(result);
        case Z3_BV_SORT:
            return this->evalBv(result, Z3_get_bv_sort_size(mZ3Context, sort));
        case Z3_FLOATING_POINT_SORT:
            return this->evalFloat(result, this->getFloatPrecision(sort));
        case Z3_ARRAY_SORT:
            return this->evalConstantArray(result, llvm::cast<ArrayType>(this->sortToType(sort)));
        default:
            llvm_unreachable("Unknown Z3 sort!");
    }
}

auto Z3Model::evalBoolean(Z3AstHandle ast) -> ExprRef<BoolLiteralExpr> 
{
    auto result = Z3_get_bool_value(mZ3Context, ast);
    switch (result) {
        case Z3_L_TRUE:  return BoolLiteralExpr::True(mContext);
        case Z3_L_FALSE: return BoolLiteralExpr::False(mContext);
        case Z3_L_UNDEF:
            llvm_unreachable("A function of boolean sort must be convertible to a boolean value!");
            return nullptr;
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
        Z3_ast newAst;
        Z3_model_eval(mZ3Context, mModel,
            Z3_mk_fpa_to_ieee_bv(mZ3Context, ast),
            false,
            &newAst);

        llvm::APInt intVal(fltTy.getWidth(), Z3_get_numeral_string(mZ3Context, newAst), 10);
        result = llvm::APFloat(semantics, intVal);
    }

    return FloatLiteralExpr::Get(fltTy, result);
}

auto Z3Model::evalConstantArray(Z3AstHandle ast, ArrayType& type) -> ExprRef<AtomicExpr>
{
    if (Z3_get_ast_kind(mZ3Context, ast) == Z3_APP_AST) {
        Z3_app app = Z3_to_app(mZ3Context, ast);
        Z3Handle<Z3_func_decl> decl(mZ3Context, Z3_get_app_decl(mZ3Context, app));

        auto declKind = Z3_get_decl_kind(mZ3Context, decl);
        ArrayLiteralExpr::MappingT map;

        while (declKind == Z3_OP_STORE) {
            Z3AstHandle index(mZ3Context, Z3_get_app_arg(mZ3Context, app, 1));
            Z3AstHandle value(mZ3Context, Z3_get_app_arg(mZ3Context, app, 2));

            ast = Z3AstHandle(mZ3Context, Z3_get_app_arg(mZ3Context, app, 0));
            app = Z3_to_app(mZ3Context, ast);
            decl = Z3Handle<Z3_func_decl>(mZ3Context, Z3_get_app_decl(mZ3Context, app));
            declKind = Z3_get_decl_kind(mZ3Context, decl);

            auto indexExpr = this->evalAst(index);
            auto valueExpr = this->evalAst(value);

            if (!llvm::isa<LiteralExpr>(indexExpr) || !llvm::isa<LiteralExpr>(valueExpr)) {
                // Something went wrong here, return an Undef expression
                return UndefExpr::Get(type);
            }

            map[llvm::cast<LiteralExpr>(indexExpr)] = llvm::cast<LiteralExpr>(valueExpr);
        }

        ExprRef<LiteralExpr> arrayDefault = nullptr;
        if (declKind == Z3_OP_CONST_ARRAY) {
            Z3AstHandle defaultValue(mZ3Context, Z3_get_app_arg(mZ3Context, app, 0));
            auto defaultExpr = this->evalAst(defaultValue);
            if (llvm::isa<LiteralExpr>(defaultExpr)) {
                arrayDefault = expr_cast<LiteralExpr>(defaultExpr);
            }
        }

        if (arrayDefault == nullptr) {
            // We do not have a default value, just bail out and return an undef.
            return UndefExpr::Get(type);
        }

        return ArrayLiteralExpr::Get(type, map, arrayDefault);
    }


    return nullptr;
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

auto Z3Model::sortToType(Z3Handle<Z3_sort> sort) -> Type&
{
    Z3_sort_kind kind = Z3_get_sort_kind(mZ3Context, sort);

    switch (kind) {
        case Z3_BOOL_SORT:
            return BoolType::Get(mContext);
        case Z3_INT_SORT:
            return IntType::Get(mContext);
        case Z3_BV_SORT:
            return BvType::Get(mContext, Z3_get_bv_sort_size(mZ3Context, sort));
        case Z3_FLOATING_POINT_SORT:
            return FloatType::Get(mContext, this->getFloatPrecision(sort));
        case Z3_ARRAY_SORT: {
            auto domain = Z3Handle<Z3_sort>(mZ3Context, Z3_get_array_sort_domain(mZ3Context, sort));
            auto range = Z3Handle<Z3_sort>(mZ3Context, Z3_get_array_sort_range(mZ3Context, sort));

            return ArrayType::Get(sortToType(domain), sortToType(range));
        }
        default:
            llvm_unreachable("Unknown/unsupported Z3 sort!");
    }
}