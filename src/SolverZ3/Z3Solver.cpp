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

#include "gazer/Support/Float.h"

#include <llvm/ADT/SmallString.h>

#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Debug.h>

#define DEBUG_TYPE "Z3Solver"

using namespace gazer;

namespace
{
    llvm::cl::opt<bool> Z3SolveParallel("z3-solve-parallel", llvm::cl::desc("Enable Z3's parallel solver"));
    llvm::cl::opt<int>  Z3ThreadsMax("z3-threads-max", llvm::cl::desc("Maximum number of threads"), llvm::cl::init(0));
    llvm::cl::opt<bool> Z3DumpModel("z3-dump-model", llvm::cl::desc("Dump Z3 model"));
} // end anonymous namespace


// Z3Solver implementation
//===----------------------------------------------------------------------===//
Z3Solver::Z3Solver(GazerContext& context)
    : Solver(context), mTransformer(mZ3Context, mTmpCount, mCache, mDecls)
{
    mConfig = Z3_mk_config();

    Z3_set_param_value(mConfig, "model", "true");
    Z3_set_param_value(mConfig, "proof", "false");
    if (Z3SolveParallel) {
        Z3_set_param_value(mConfig, "parallel.enable", "true");
        if (Z3ThreadsMax != 0) {
            Z3_set_param_value(mConfig, "threads.max", std::to_string(Z3ThreadsMax).c_str());
        }
    }

    mZ3Context = Z3_mk_context_rc(mConfig);
    mSolver = Z3_mk_solver(mZ3Context);
    Z3_solver_inc_ref(mZ3Context, mSolver);
}

Z3Solver::~Z3Solver()
{
    mCache.clear();
    mDecls.clear();
    mTransformer.clear();
    Z3_solver_dec_ref(mZ3Context, mSolver);
    Z3_del_context(mZ3Context);
    Z3_del_config(mConfig);
}

Solver::SolverStatus Z3Solver::run()
{
    Z3_lbool result =  Z3_solver_check(mZ3Context, mSolver);

    switch (result) {
        case Z3_L_FALSE: return SolverStatus::UNSAT;
        case Z3_L_TRUE:
            if (Z3DumpModel) {
                llvm::errs() << Z3_model_to_string(mZ3Context, Z3_solver_get_model(mZ3Context, mSolver)) << "\n";
            }
            return SolverStatus::SAT;
        case Z3_L_UNDEF: return SolverStatus::UNKNOWN;
    }

    llvm_unreachable("Unknown solver status encountered.");
}

void Z3Solver::addConstraint(ExprPtr expr)
{
    auto z3Expr = mTransformer.walk(expr);
    Z3_solver_assert(mZ3Context, mSolver, z3Expr);
}

void Z3Solver::reset()
{
    mCache.clear();
    mDecls.clear();
    Z3_solver_reset(mZ3Context, mSolver);
}

void Z3Solver::push()
{
    mCache.push();
    mDecls.push();
    Z3_solver_push(mZ3Context, mSolver);
}

void Z3Solver::pop()
{
    mCache.pop();
    mDecls.pop();
    Z3_solver_pop(mZ3Context, mSolver, 1);
}

void Z3Solver::printStats(llvm::raw_ostream& os)
{
    auto stats = Z3_solver_get_statistics(mZ3Context, mSolver);
    Z3_stats_inc_ref(mZ3Context, stats);
    os << Z3_stats_to_string(mZ3Context, stats);
    Z3_stats_dec_ref(mZ3Context, stats);
}

void Z3Solver::dump(llvm::raw_ostream& os)
{
    os << Z3_solver_to_string(mZ3Context, mSolver);
}

template<class T> static void checkErrors(Z3_context ctx, T node)
{
    if (node == nullptr) {
        std::string errStr;
        llvm::raw_string_ostream err{errStr};

        err << "Invalid Z3 node!\n"
            << "Z3 error: " << Z3_get_error_msg(ctx, Z3_get_error_code(ctx))
            << "\n";
        llvm::report_fatal_error(err.str(), true);
    }
}

// Z3ExprTransformer implementation
//===----------------------------------------------------------------------===//
auto Z3ExprTransformer::createHandle(Z3_ast ast) -> Z3AstHandle
{
    checkErrors(mZ3Context, ast);
    return Z3AstHandle{mZ3Context, ast};
}

auto Z3ExprTransformer::shouldSkip(const ExprPtr& expr, Z3AstHandle* ret) -> bool
{
    if (expr->isNullary() || expr->isUnary()) {
        return false;
    }

    auto result = mCache.get(expr);
    if (result) {
        *ret = *result;
        return true;
    }

    return false;
}

void Z3ExprTransformer::handleResult(const ExprPtr& expr, Z3AstHandle& ret)
{
    mCache.insert(expr, ret);
}

auto Z3ExprTransformer::translateDecl(Variable* variable) -> Z3Handle<Z3_func_decl>
{
    auto opt = mDecls.get(variable);
    if (opt) {
        return *opt;
    }

    auto name = Z3_mk_string_symbol(mZ3Context, variable->getName().c_str());
    auto decl = Z3_mk_func_decl(mZ3Context, name, 0, nullptr, typeToSort(variable->getType()));
    checkErrors(mZ3Context, decl);

    auto handle = Z3Handle<Z3_func_decl>(mZ3Context, decl);
    mDecls.insert(variable, handle);

    return handle;
}

auto Z3ExprTransformer::visitVarRef(const ExprRef<VarRefExpr>& expr) -> Z3AstHandle
{
    auto decl = this->translateDecl(&expr->getVariable());
    return createHandle(Z3_mk_app(mZ3Context, decl, 0, nullptr));
}

auto Z3ExprTransformer::visitAdd(const ExprRef<AddExpr>& expr) -> Z3AstHandle
{
    if (expr->getType().isBvType()) {
        return createHandle(Z3_mk_bvadd(mZ3Context, getOperand(0), getOperand(1)));
    }

    std::array<Z3_ast, 2> ops = { getOperand(0), getOperand(1) };
    return createHandle(Z3_mk_add(mZ3Context, 2, ops.data()));
}

auto Z3ExprTransformer::visitSub(const ExprRef<SubExpr>& expr) -> Z3AstHandle
{
    if (expr->getType().isBvType()) {
        return createHandle(Z3_mk_bvsub(mZ3Context, getOperand(0), getOperand(1)));
    }

    std::array<Z3_ast, 2> ops = { getOperand(0), getOperand(1) };
    return createHandle(Z3_mk_sub(mZ3Context, 2, ops.data()));
}

auto Z3ExprTransformer::visitMul(const ExprRef<MulExpr>& expr) -> Z3AstHandle
{
    if (expr->getType().isBvType()) {
        return createHandle(Z3_mk_bvmul(mZ3Context, getOperand(0), getOperand(1)));
    }

    std::array<Z3_ast, 2> ops = { getOperand(0), getOperand(1) };
    return createHandle(Z3_mk_mul(mZ3Context, 2, ops.data()));
}

auto Z3ExprTransformer::visitAnd(const ExprRef<AndExpr>& expr) -> Z3AstHandle
{
    z3::array<Z3_ast> ops(expr->getNumOperands());
    for (size_t i = 0; i < ops.size(); ++i) {
        ops[i] = getOperand(i);
    }

    return createHandle(Z3_mk_and(mZ3Context, expr->getNumOperands(), ops.ptr()));
}

auto Z3ExprTransformer::visitOr(const ExprRef<OrExpr>& expr) -> Z3AstHandle
{
    z3::array<Z3_ast> ops(expr->getNumOperands());
    for (size_t i = 0; i < ops.size(); ++i) {
        ops[i] = getOperand(i);
    }

    return createHandle(Z3_mk_or(mZ3Context, expr->getNumOperands(), ops.ptr()));
}

auto Z3ExprTransformer::visitNotEq(const ExprRef<NotEqExpr>& expr) -> Z3AstHandle
{
    std::array<Z3_ast, 2> ops = { getOperand(0), getOperand(1) };
    return createHandle(Z3_mk_distinct(mZ3Context, 2, ops.data()));
}

auto Z3ExprTransformer::visitTupleSelect(const ExprRef<TupleSelectExpr>& expr) -> Z3AstHandle
{
    auto& tupTy = llvm::cast<TupleType>(expr->getOperand(0)->getType());
    
    assert(mTupleInfo.count(&tupTy) != 0 && "Tuple types should have been handled before!");
    auto& info = mTupleInfo[&tupTy];

    auto& proj = info.projections[expr->getIndex()];
    std::array<Z3_ast, 1> args = { getOperand(0) };

    return createHandle(Z3_mk_app(mZ3Context, proj, 1, args.data()));
}

auto Z3ExprTransformer::visitTupleConstruct(const ExprRef<TupleConstructExpr>& expr) -> Z3AstHandle
{
    auto& tupTy = llvm::cast<TupleType>(expr->getOperand(0)->getType());
    
    assert(mTupleInfo.count(&tupTy) != 0 && "Tuple types should have been handled before!");
    auto& info = mTupleInfo[&tupTy];

    std::vector<Z3_ast> args(tupTy.getNumSubtypes());
    for (size_t i = 0; i < tupTy.getNumSubtypes(); ++i) {
        args[i] = getOperand(i);
    }

    return createHandle(Z3_mk_app(mZ3Context, info.constructor, tupTy.getNumSubtypes(), args.data()));
}

auto Z3ExprTransformer::transformRoundingMode(llvm::APFloat::roundingMode rm) -> Z3AstHandle
{
    switch (rm) {
        case llvm::APFloat::roundingMode::rmNearestTiesToEven:
            return createHandle(Z3_mk_fpa_round_nearest_ties_to_even(mZ3Context));
        case llvm::APFloat::roundingMode::rmNearestTiesToAway:
            return createHandle(Z3_mk_fpa_round_nearest_ties_to_away(mZ3Context));
        case llvm::APFloat::roundingMode::rmTowardPositive:
            return createHandle(Z3_mk_fpa_round_toward_positive(mZ3Context));
        case llvm::APFloat::roundingMode::rmTowardNegative:
            return createHandle(Z3_mk_fpa_round_toward_negative(mZ3Context));
        case llvm::APFloat::roundingMode::rmTowardZero:
            return createHandle(Z3_mk_fpa_round_toward_zero(mZ3Context));
    }

    llvm_unreachable("Invalid rounding mode");
}

Z3Handle<Z3_sort> Z3ExprTransformer::typeToSort(Type& type)
{
    switch (type.getTypeID()) {
        case Type::BoolTypeID:
            return Z3Handle<Z3_sort>{mZ3Context, Z3_mk_bool_sort(mZ3Context)};
        case Type::IntTypeID:
            return Z3Handle<Z3_sort>{mZ3Context, Z3_mk_int_sort(mZ3Context)};
        case Type::BvTypeID: {
            auto& intTy = llvm::cast<BvType>(type);
            return Z3Handle<Z3_sort>{mZ3Context, Z3_mk_bv_sort(mZ3Context, intTy.getWidth())};
        }
        case Type::RealTypeID:
            return Z3Handle<Z3_sort>{mZ3Context, Z3_mk_real_sort(mZ3Context)};
        case Type::FloatTypeID: {
            auto& fltTy = llvm::cast<FloatType>(type);
            switch (fltTy.getPrecision()) {
                case FloatType::Half:
                    return Z3Handle<Z3_sort>(mZ3Context, Z3_mk_fpa_sort_half(mZ3Context));
                case FloatType::Single:
                    return Z3Handle<Z3_sort>(mZ3Context, Z3_mk_fpa_sort_single(mZ3Context));
                case FloatType::Double:
                    return Z3Handle<Z3_sort>(mZ3Context, Z3_mk_fpa_sort_double(mZ3Context));
                case FloatType::Quad:
                    return Z3Handle<Z3_sort>(mZ3Context, Z3_mk_fpa_sort_quadruple(mZ3Context));
            }
            llvm_unreachable("Invalid floating-point precision");
        }
        case Type::ArrayTypeID: {
            auto& arrTy = llvm::cast<ArrayType>(type);
            return Z3Handle<Z3_sort>(mZ3Context, Z3_mk_array_sort(mZ3Context,
                typeToSort(arrTy.getIndexType()),
                typeToSort(arrTy.getElementType())
            ));
        }
        case Type::TupleTypeID:
            return this->handleTupleType(llvm::cast<TupleType>(type));
    }

    llvm_unreachable("Unsupported gazer type for Z3Solver");
}

Z3Handle<Z3_sort> Z3ExprTransformer::handleTupleType(TupleType& tupTy)
{
    // Check if we already visited this type
    auto it = mTupleInfo.find(&tupTy);
    if (it != mTupleInfo.end()) {
        return it->second.sort;
    }

    auto name = Z3_mk_string_symbol(mZ3Context, ("mk_" + tupTy.getName()).c_str());

    std::vector<Z3_symbol> projs;
    std::vector<Z3_sort> sorts;

    for (size_t i = 0; i < tupTy.getNumSubtypes(); ++i) {
        std::string projName = tupTy.getName() + "_" + std::to_string(i);
        projs.emplace_back(Z3_mk_string_symbol(mZ3Context, projName.c_str()));
        sorts.emplace_back(typeToSort(tupTy.getTypeAtIndex(i)));
    }

    Z3_func_decl constructDecl;
    std::vector<Z3_func_decl> projDecls(tupTy.getNumSubtypes());

    auto tup = Z3_mk_tuple_sort(
        mZ3Context, name, tupTy.getNumSubtypes(), projs.data(),
        sorts.data(), &constructDecl, projDecls.data()
    );

    if (tup == nullptr) {
        std::string errStr;
        llvm::raw_string_ostream err{errStr};

        err << "Invalid Z3_sort!\n"
            << "Z3 error: " << Z3_get_error_msg(mZ3Context, Z3_get_error_code(mZ3Context))
            << "\n";
        llvm::report_fatal_error(err.str(), true);
    }

    auto& info = mTupleInfo[&tupTy];

    info.constructor = Z3Handle<Z3_func_decl>(mZ3Context, constructDecl);
    for (size_t i = 0; i < tupTy.getNumSubtypes(); ++i) {
        info.projections.emplace_back(Z3Handle<Z3_func_decl>(mZ3Context, projDecls[i]));
    }
    info.sort = Z3Handle<Z3_sort>(mZ3Context, tup);

    return info.sort;
}

Z3AstHandle Z3ExprTransformer::translateLiteral(const ExprRef<LiteralExpr>& expr)
{
    if (auto bvLit = llvm::dyn_cast<BvLiteralExpr>(expr)) {
        llvm::SmallString<20> buffer;
        bvLit->getValue().toStringUnsigned(buffer, 10);
        return createHandle(Z3_mk_numeral(mZ3Context, buffer.c_str(), typeToSort(bvLit->getType())));
    }
    
    if (auto bl = llvm::dyn_cast<BoolLiteralExpr>(expr)) {
        return createHandle(bl->getValue() ? Z3_mk_true(mZ3Context) : Z3_mk_false(mZ3Context));
    }
    
    if (auto il = llvm::dyn_cast<IntLiteralExpr>(expr)) {
        return createHandle(Z3_mk_int64(mZ3Context, il->getValue(), Z3_mk_int_sort(mZ3Context)));
    }
    
    if (auto fl = llvm::dyn_cast<FloatLiteralExpr>(expr)) {        
        if (fl->getType().getPrecision() == FloatType::Single) {
            return createHandle(Z3_mk_fpa_numeral_float(
                mZ3Context, fl->getValue().convertToFloat(), typeToSort(fl->getType())
            ));
        }
        
        if (fl->getType().getPrecision() == FloatType::Double) {
            return createHandle(Z3_mk_fpa_numeral_double(
                mZ3Context, fl->getValue().convertToDouble(), typeToSort(fl->getType())
            ));
        }
    }

    if (auto arrayLit = llvm::dyn_cast<ArrayLiteralExpr>(expr)) {
        Z3AstHandle ast;            
        if (arrayLit->hasDefault()) {
            ast = createHandle(Z3_mk_const_array(
                mZ3Context,
                typeToSort(arrayLit->getType().getIndexType()),
                this->translateLiteral(arrayLit->getDefault())
            ));
        } else {
            ast = createHandle(Z3_mk_fresh_const(
                mZ3Context,
                "",
                typeToSort(arrayLit->getType())
            ));
        }

        Z3AstHandle result = ast;
        for (auto& [index, elem] : arrayLit->getMap()) {
            result = createHandle(Z3_mk_store(
                mZ3Context, result, this->translateLiteral(index), this->translateLiteral(elem)
            ));
        }

        return result;
    }

    llvm_unreachable("Unsupported operand type.");
}

std::unique_ptr<Solver> Z3SolverFactory::createSolver(GazerContext& context)
{
    return std::unique_ptr<Solver>(new Z3Solver(context));
}
