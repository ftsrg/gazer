#include "gazer/Core/Expr.h"
#include "gazer/Core/ExprTypes.h"

#include "GazerContextImpl.h"

#include <llvm/Support/raw_ostream.h>
#include <gazer/Core/ExprTypes.h>

using namespace gazer;

//----------------- Basic utilities for expression handling -----------------//
// These functions contain basic ExprKind-dependent functionality which should
// be updated if a new expression kind is introduced to the system.

std::size_t gazer::expr_kind_prime(Expr::ExprKind kind)
{
    // Unique prime numbers for each expression kind, used for hashing.
    static constexpr std::array ExprKindPrimes = {
        472127u, 167159u, 682183u, 616243u, 644431u, 978647u, 788959u, 200891u,
        938939u, 537679u, 757711u, 132697u, 195203u, 511193u, 286249u, 178481u,
        956057u, 614531u, 360233u, 621913u, 758041u, 718559u, 930991u, 686201u,
        465977u, 765007u, 388727u, 730819u, 134353u, 819583u, 314953u, 848633u,
        290623u, 241291u, 579499u, 384287u, 125287u, 920273u, 485833u, 326449u,
        972683u, 485167u, 882599u, 535727u, 383651u, 159833u, 796001u, 218479u,
        163993u, 622561u, 938881u, 692467u, 851971u, 478427u
    };

    static_assert(
        ExprKindPrimes.size() == Expr::LastExprKind + 1,
        "Missing ExprKind in GetExprKindPrime!"
    );

    if (Expr::FirstExprKind <= kind && kind <= Expr::LastExprKind) {
        return ExprKindPrimes[kind];
    }

    llvm_unreachable("Invalid expression kind.");
};

//------------------- Expression creation and destruction -------------------//

Expr::Expr(Expr::ExprKind kind, Type &type)
    : mKind(kind), mType(type), mRefCount(0)
{}

void Expr::DeleteExpr(gazer::Expr *expr)
{
    assert(expr != nullptr && "Attempting to remove null expression!");

    if (llvm::isa<BoolLiteralExpr>(expr)) {
        // These expression classes are allocated separately from the rest,
        // therefore they need to be cleaned up differently.
        delete expr;
    } else {
        expr->getContext().pImpl->Exprs.destroy(expr);
    }
}

size_t Expr::getHashCode() const {
    return mHashCode;
}

//----------------------- Subtype initializers ------------------------//

auto NotExpr::Create(const ExprPtr& operand) -> ExprRef<NotExpr>
{
    assert(operand->getType().isBoolType() && "NotExpr operand must be boolean!");
    auto& context = operand->getContext();

    return context.pImpl->Exprs.create<NotExpr>(BoolType::Get(context), { operand });
}

template<Expr::ExprKind Kind>
auto ExtCastExpr<Kind>::Create(const ExprPtr& operand, Type& type) -> ExprRef<ExtCastExpr<Kind>>
{
    assert(operand->getType().isBvType() && "Can only do bitwise cast on bit vectors!");
    assert(type.isBvType() && "Can only bitwise cast to a bit vector type!");

    auto lhsTy = llvm::dyn_cast<BvType>(&operand->getType());
    auto rhsTy = llvm::dyn_cast<BvType>(&type);

    assert((rhsTy->getWidth() > lhsTy->getWidth()) && "Extend casts must increase bit width!");
    auto& context = lhsTy->getContext();

    return context.pImpl->Exprs.create<ExtCastExpr<Kind>>(*rhsTy, { operand });
}

auto ExtractExpr::Create(const ExprPtr& operand, unsigned offset, unsigned width) -> ExprRef<ExtractExpr>
{
    auto opTy = llvm::dyn_cast<BvType>(&operand->getType());

    assert(opTy != nullptr && "Can only do bitwise cast on integers!");
    assert(width > 0 && "Can only extract at least one bit!");
    assert(opTy->getWidth() > width + offset && "Extracted bit vector must be smaller than the original!");
    auto& context = opTy->getContext();

    return context.pImpl->Exprs.create<ExtractExpr>(
        BvType::Get(context, width), { operand }, offset, width
    );
}

static constexpr bool is_arithmetic_only(Expr::ExprKind kind) {
    return kind == Expr::Div;
}

static constexpr bool is_bv_only(Expr::ExprKind kind) {
    return Expr::BvSDiv <= kind && kind <= Expr::BvXor;
}

template<Expr::ExprKind Kind>
auto ArithmeticExpr<Kind>::Create(const ExprPtr& left, const ExprPtr& right) -> ExprRef<ArithmeticExpr<Kind>>
{
    auto& leftTy = left->getType();
    assert(leftTy == right->getType() && "Arithmetic expression operand types must match!");
    if constexpr (is_bv_only(Kind)) {
        assert(leftTy.isBvType() && "Can only perform bitvector arithmetic on Bv types!");
    } else if constexpr (is_arithmetic_only(Kind)) {
        assert(leftTy.isArithmetic() && "Can only perform bitvector arithmetic on Bv types!");
    } else {
        assert(
            (leftTy.isBvType() || leftTy.isIntType() || leftTy.isRealType())
            && "Can only perform arithmetic operations on Bv, Int or Real types!"
        );
    }
    auto& context = left->getContext();

    return context.pImpl->Exprs.create<ArithmeticExpr<Kind>>(left->getType(), { left, right });
}

template<Expr::ExprKind Kind>
auto CompareExpr<Kind>::Create(const ExprPtr& left, const ExprPtr& right) -> ExprRef<CompareExpr<Kind>>
{
    assert(left->getType() == right->getType() && "Compare expresison operand types must match!");
    auto& context = left->getContext();

    return context.pImpl->Exprs.create<CompareExpr<Kind>>(BoolType::Get(context), { left, right });
}

template<Expr::ExprKind Kind, class InputIterator>
static auto CreateMultiaryExpr(InputIterator begin, InputIterator end) -> ExprRef<MultiaryLogicExpr<Kind>>
{
    assert(std::all_of(begin, end, [](const ExprPtr& e) { return e->getType().isBoolType(); })
        && "Operands of a multiary logic expression must booleans!"
    );
    assert(begin != end && "Multiary expression operand list must not be empty!");
    GazerContext& context = (*begin)->getContext();

    return context.pImpl->Exprs.createRange<MultiaryLogicExpr<Kind>>(
        BoolType::Get(context),
        begin, end
    );
}

template<Expr::ExprKind Kind>
auto MultiaryLogicExpr<Kind>::Create(const ExprPtr& left, const ExprPtr& right) -> ExprRef<MultiaryLogicExpr<Kind>>
{
    return Create(ExprVector{left, right});
}

template<Expr::ExprKind Kind>
auto MultiaryLogicExpr<Kind>::Create(const ExprVector &ops) -> ExprRef<MultiaryLogicExpr<Kind>>
{
    return CreateMultiaryExpr<Kind>(ops.begin(), ops.end());
}

template<Expr::ExprKind Kind>
auto BinaryLogicExpr<Kind>::Create(const ExprPtr& left, const ExprPtr& right) -> ExprRef<BinaryLogicExpr<Kind>>
{
    assert(left->getType().isBoolType() && "Can only apply binary logic to boolean expressions.");
    assert(right->getType().isBoolType() && "Can only apply binary logic to boolean expressions.");
    auto& context = left->getContext();

    return context.pImpl->Exprs.create<BinaryLogicExpr<Kind>>(left->getType(), { left, right });
}

template<Expr::ExprKind Kind>
auto FpQueryExpr<Kind>::Create(const ExprPtr& operand) -> ExprRef<FpQueryExpr<Kind>>
{
    assert(operand->getType().isFloatType() && "FpQuery requrires a float operand!");
    auto& context = operand->getContext();

    return context.pImpl->Exprs.create<FpQueryExpr<Kind>>(BoolType::Get(context), { operand });
}

static constexpr bool is_bv_to_fp(Expr::ExprKind Kind) { return Kind == Expr::UnsignedToFp || Kind == Expr::SignedToFp; }
static constexpr bool is_fp_to_bv(Expr::ExprKind Kind) { return Kind == Expr::FpToUnsigned || Kind == Expr::FpToSigned; }

template<Expr::ExprKind Kind>
auto BvFpCastExpr<Kind>::Create(const ExprPtr& operand, Type& type, const llvm::APFloat::roundingMode& rm) -> ExprRef<BvFpCastExpr<Kind>>
{
    assert(operand->getType() != type && "Cast source and target operands must differ!");

    if constexpr (is_bv_to_fp(Kind)) {
        assert(operand->getType().isBvType() && "Can only do BvToFp cast on bitvector inputs!");
        assert(type.isFloatType() && "Can only do BvToFp casts to floating-point targets!");
    } else if constexpr (is_fp_to_bv(Kind)) {
        assert(operand->getType().isFloatType() && "Can only do FpToBv cast on floating-point inputs!");
        assert(type.isBvType() && "Can only do FpToBv casts to bitvector targets!");
    } else if constexpr (Kind == Expr::FCast) {
        assert(operand->getType().isFloatType() && "Can only do FCast cast on float inputs!");
        assert(type.isFloatType() && "Can only do FCast casts to floating-point targets!");

        auto& fltTy = *llvm::cast<FloatType>(&operand->getType());
        auto& targetTy = *llvm::cast<FloatType>(&type);

        assert((fltTy != targetTy) && "FCast casts must change the target type!");
    }

    auto& context = operand->getContext();

    return context.pImpl->Exprs.create<BvFpCastExpr<Kind>>(type, { operand }, rm);
}

template<Expr::ExprKind Kind>
auto FpArithmeticExpr<Kind>::Create(const ExprPtr& left, const ExprPtr& right, const llvm::APFloat::roundingMode& rm) -> ExprRef<FpArithmeticExpr<Kind>>
{
    assert(left->getType().isFloatType() && "Can only define floating-point operations on float types!");
    assert(left->getType() == right->getType() && "Arithmetic expression operand types must match!");
    auto& context = left->getContext();

    return context.pImpl->Exprs.create<FpArithmeticExpr<Kind>>(left->getType(), { left, right }, rm);
}

template<Expr::ExprKind Kind>
auto FpCompareExpr<Kind>::Create(const ExprPtr& left, const ExprPtr& right) -> ExprRef<FpCompareExpr<Kind>>
{
    assert(left->getType().isFloatType() && "Floating-point compare expressions must have a float operand!");
    assert(right->getType().isFloatType() && "Floating-point compare expressions must have a float operand!");
    auto& context = left->getContext();

    return context.pImpl->Exprs.create<FpCompareExpr<Kind>>(BoolType::Get(context), { left, right });
}

auto SelectExpr::Create(const ExprPtr& condition, const ExprPtr& then, const ExprPtr& elze) -> ExprRef<SelectExpr>
{
    assert(then->getType() == elze->getType() && "Select expression operand types must match.");
    assert(condition->getType().isBoolType() && "Select expression condition type must be boolean.");
    auto& context = condition->getContext();

    return context.pImpl->Exprs.create<SelectExpr>(then->getType(), { condition, then, elze });
}

ExprRef<ArrayReadExpr> ArrayReadExpr::Create(
    ExprPtr array, ExprPtr index
) {
    assert(array->getType().isArrayType() && "ArrayRead only works on arrays.");
    ArrayType* arrTy = llvm::cast<ArrayType>(&array->getType());
    assert(arrTy->getIndexType() == index->getType() &&
        "Array index type and index types must match.");

    auto& context = arrTy->getContext();
    return context.pImpl->Exprs.create<ArrayReadExpr>(arrTy->getElementType(), { array, index });
}

ExprRef<ArrayWriteExpr> ArrayWriteExpr::Create(
    ExprPtr array, ExprPtr index, ExprPtr value
) {
    assert(array->getType().isArrayType() && "ArrayRead only works on arrays.");
    ArrayType* arrTy = llvm::cast<ArrayType>(&array->getType());
    assert(arrTy->getIndexType() == index->getType() &&
        "Array index type and index types must match.");
    auto& context = arrTy->getContext();

    return context.pImpl->Exprs.create<ArrayWriteExpr>(*arrTy, { array, index, value });
}


namespace gazer
{
template class ExtCastExpr<Expr::ZExt>;
template class ExtCastExpr<Expr::SExt>;
template class ArithmeticExpr<Expr::Add>;
template class ArithmeticExpr<Expr::Sub>;
template class ArithmeticExpr<Expr::Mul>;
template class ArithmeticExpr<Expr::Div>;
template class ArithmeticExpr<Expr::BvSDiv>;
template class ArithmeticExpr<Expr::BvUDiv>;
template class ArithmeticExpr<Expr::BvSRem>;
template class ArithmeticExpr<Expr::BvURem>;
template class ArithmeticExpr<Expr::Shl>;
template class ArithmeticExpr<Expr::LShr>;
template class ArithmeticExpr<Expr::AShr>;
template class ArithmeticExpr<Expr::BvAnd>;
template class ArithmeticExpr<Expr::BvOr>;
template class ArithmeticExpr<Expr::BvXor>;

template class CompareExpr<Expr::Eq>;
template class CompareExpr<Expr::NotEq>;
template class CompareExpr<Expr::BvSLt>;
template class CompareExpr<Expr::BvSLtEq>;
template class CompareExpr<Expr::BvSGt>;
template class CompareExpr<Expr::BvSGtEq>;
template class CompareExpr<Expr::BvULt>;
template class CompareExpr<Expr::BvULtEq>;
template class CompareExpr<Expr::BvUGt>;
template class CompareExpr<Expr::BvUGtEq>;
template class MultiaryLogicExpr<Expr::And>;
template class MultiaryLogicExpr<Expr::Or>;
template class BinaryLogicExpr<Expr::Xor>;
template class BinaryLogicExpr<Expr::Imply>;

template class FpQueryExpr<Expr::FIsNan>;
template class FpQueryExpr<Expr::FIsInf>;
template class BvFpCastExpr<Expr::FCast>;
template class BvFpCastExpr<Expr::SignedToFp>;
template class BvFpCastExpr<Expr::UnsignedToFp>;
template class BvFpCastExpr<Expr::FpToSigned>;
template class BvFpCastExpr<Expr::FpToUnsigned>;

template class FpArithmeticExpr<Expr::FAdd>;
template class FpArithmeticExpr<Expr::FSub>;
template class FpArithmeticExpr<Expr::FMul>;
template class FpArithmeticExpr<Expr::FDiv>;
template class FpCompareExpr<Expr::FEq>;
template class FpCompareExpr<Expr::FGt>;
template class FpCompareExpr<Expr::FGtEq>;
template class FpCompareExpr<Expr::FLt>;
template class FpCompareExpr<Expr::FLtEq>;
} // end namespace gazer

//------------------------------- Utilities ---------------------------------//

llvm::StringRef Expr::getKindName(ExprKind kind)
{
    #define GAZER_EXPR_KIND(KIND) case KIND: return #KIND;

    /// This array contains the name of every Gazer expression kind.
    switch (kind) {
        #include "gazer/Core/Expr/ExprKind.inc"
    };

    #undef GAZER_EXPR_KIND

    llvm_unreachable("Invalid expression kind.");
}

void NonNullaryExpr::print(llvm::raw_ostream& os) const
{
    size_t i = 0;
    os << getType().getName() << " " << Expr::getKindName(getKind()) << "(";
    while (i < getNumOperands() - 1) {
        getOperand(i)->print(os);
        os << ",";
        ++i;
    }

    getOperand(i)->print(os);
    os << ")";
}

void ExtractExpr::print(llvm::raw_ostream& os) const
{
    os << getType().getName() << " " << Expr::getKindName(getKind()) << "(";
    getOperand()->print(os);
    os << ", " << mOffset << ", " << mWidth << ")";
}

llvm::raw_ostream& gazer::operator<<(llvm::raw_ostream& os, const Expr& expr)
{
    expr.print(os);
    return os;
}