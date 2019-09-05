/// 
/// \file This file describes the pattern matching utility for expressions.
/// The matcher system is tree-based and works with ExprPtr parameters.
/// This provides a powerful and intuitive tool for pattern-based expression
/// rewriting. See this code snippet for an usage example:
/// 
///     ExprPtr input = ...
///     ExprPtr f1, f2, f3;
///     if (match(input, m_And(
///         m_Or(m_Expr(f1), m_Expr(f2)),
///         m_Or(m_Specific(f1), m_Expr(f3))
///     )) {
///         // (F1 | F2) & (F1 | F3) -> F1 & (F2 | F3)
///         return AndExpr::Create(f1, OrExpr::Create(f2, f3));
///     }
/// 
/// Binary and multiary patterns are always evaluated left to right.
/// Multiary patterns come in different flavors, depending on the use case.
/// For example, in the case of the And operator, the following patterns
/// are available:
/// 
///     \li \c m_And(P1, P2, ..., Pn): matches an AndExpr having precisely
///       n operands, and matches every Pi pattern against any operand.
///       The match is successful if every Pi pattern is matched.
///       If multiple operands match a pattern, only the leftmost operand is
///       matched and bound. If a single operand matches multiple patterns,
///       only the leftmost pattern is matched and bound.
/// 
///     \li \c m_And(U, P1, P2, ..., Pk): matches an AndExpr having
///       at least k operands, matches every Pi pattern against any operand
///       (with the same semantics described above) and fills the ExprVector U
///       with the unmatched operands.
/// 
/// The implementation in this file is mostly based on the Value matcher
/// mechanism found in LLVM.
/// 
#ifndef GAZER_CORE_EXPR_MATCHER_H
#define GAZER_CORE_EXPR_MATCHER_H

#include "gazer/Core/Expr.h"
#include "gazer/Core/ExprTypes.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/Support/Casting.h>

#include <bitset>

namespace gazer { namespace PatternMatch {

/// Match a given expression to a pattern
template<typename ExprTy, typename Pattern>
inline bool match(const ExprRef<ExprTy>& ptr, const Pattern& pattern) {
    return const_cast<Pattern&>(pattern).match(ptr);
}

/// Matches two expressions to two patterns
template<typename LTy, typename RTy>
inline bool match(ExprPtr e1, ExprPtr e2, const LTy& left, const RTy& right)
{
    return const_cast<LTy&>(left).match(e1) && const_cast<RTy&>(right).match(e2);
}

template<typename P1, typename P2, typename P3>
inline bool match(ExprPtr e1, ExprPtr e2, ExprPtr e3, const P1& p1, const P2& p2, const P3& p3)
{
    return const_cast<P1&>(p1).match(e1)
        && const_cast<P2&>(p2).match(e2)
        && const_cast<P3&>(p3).match(e3);
}

template<typename LTy, typename RTy>
inline bool unord_match(ExprPtr e1, ExprPtr e2, const LTy& left, const RTy& right)
{
    return (const_cast<LTy&>(left).match(e1) && const_cast<RTy&>(right).match(e2))
        || (const_cast<LTy&>(left).match(e2) && const_cast<RTy&>(right).match(e1));
}

namespace detail
{
    template<unsigned Current, typename... Patterns>
    inline bool vector_element_match(ExprVector& unmatched, std::tuple<Patterns...>& tuple)
    {
        if constexpr (Current < sizeof...(Patterns)) {
            auto& pattern = std::get<Current>(tuple);
            for (size_t i = 0; i < unmatched.size(); ++i) {
                if (pattern.match(unmatched[i])) {
                    // We have a match here, remove the unmatched element from the array and try more patterns.
                    unmatched.erase(unmatched.begin() + i);
                    return vector_element_match<Current + 1>(unmatched, tuple);
                }
            }

            // This pattern could not be matched, we need to return false.
            return false;
        }

        return true;
    }
} // end namespace detail

template<typename... Patterns>
inline bool unord_match(const ExprVector& vec, Patterns... patterns)
{
    ExprVector unmatched = vec;
    std::tuple<Patterns...> patternsTuple(patterns...);

    return detail::vector_element_match<0, Patterns...>(unmatched, patternsTuple);
}

template<typename... Patterns>
inline bool unord_match(const ExprVector& vec, ExprVector& unmatched, Patterns... patterns)
{
    assert(&vec != &unmatched);
    assert(unmatched.empty() && "Can only fill an empty unmatched vector!");
    std::tuple<Patterns...> patternsTuple(patterns...);

    unmatched = vec;

    return detail::vector_element_match<0, Patterns...>(unmatched, patternsTuple);
}

struct true_match
{
    template<typename InputTy>
    bool match(const ExprRef<InputTy>& ptr) { return true; }
};

template<typename ExprTy>
struct expr_match
{
    template<typename InputTy>
    bool match(const ExprRef<InputTy>& ptr) {
        return llvm::isa<ExprTy>(ptr.get());
    }
};

/// Matches an arbitrary expression and ignores it.
inline expr_match<Expr> m_Expr() { return expr_match<Expr>(); }

/// Matches an arbitrary atomic expression and ignores it.
inline expr_match<AtomicExpr> m_Atomic() { return expr_match<AtomicExpr>(); }

/// Matches an arbitray non-nullary expression and ignores it.
inline expr_match<NonNullaryExpr> m_NonNullary() { return expr_match<NonNullaryExpr>(); }

/// Matches an arbitrary undef expression and ignores it.
inline expr_match<UndefExpr> m_Undef() { return expr_match<UndefExpr>(); }

/// Matches an arbitrary literal expression and ignores it.
inline expr_match<LiteralExpr> m_Literal() { return expr_match<LiteralExpr>(); }

/// Matches an arbitrary variable reference and ignores it.
inline expr_match<VarRefExpr> m_VarRef() { return expr_match<VarRefExpr>(); }

struct apint_match
{
    llvm::APInt* const result;

    apint_match(llvm::APInt* const result) : result(result) {}

    template<typename InputTy>
    bool match(const ExprRef<InputTy>& expr)
    {
        if (auto bvLit = llvm::dyn_cast<BvLiteralExpr>(expr.get())) {
            *result = bvLit->getValue();
            return true;
        }

        return false;
    }
};

inline apint_match m_Bv(llvm::APInt* const result) {
    return apint_match(result);
}

struct specific_match
{
    const ExprPtr& storedExpr;

    specific_match(const ExprPtr& expr) : storedExpr(expr) {}

    bool match(const ExprPtr& expr) { return storedExpr == expr; }
};

inline specific_match m_Specific(const ExprPtr& expr) {
    return specific_match(expr);
}

template<typename ExprTy>
struct bind_ty
{
    ExprRef<ExprTy>& storedPtr;
    bind_ty(ExprRef<ExprTy>& ptr) : storedPtr(ptr) {}

    template<typename InputTy>
    bool match(const ExprRef<InputTy>& ptr) {
        if (auto expr = llvm::dyn_cast<ExprTy>(ptr)) {
            storedPtr = expr;
            return true;
        }

        return false;
    }
};

inline bind_ty<Expr> m_Expr(ExprRef<Expr>& ptr) { return bind_ty<Expr>(ptr); }

inline bind_ty<VarRefExpr> m_VarRef(ExprRef<VarRefExpr>& ptr) {
    return bind_ty<VarRefExpr>(ptr);
}

inline bind_ty<LiteralExpr> m_Literal(ExprRef<LiteralExpr>& ptr) {
    return bind_ty<LiteralExpr>(ptr);
}

inline bind_ty<BoolLiteralExpr> m_BoolLit(ExprRef<BoolLiteralExpr>& ptr) {
    return bind_ty<BoolLiteralExpr>(ptr);
}

inline bind_ty<BvLiteralExpr> m_BvLit(ExprRef<BvLiteralExpr>& ptr) {
    return bind_ty<BvLiteralExpr>(ptr);
}

//===------------------- Matcher for unary expressions --------------------===//
//============================================================================//

template<typename PatternTy, Expr::ExprKind Kind>
struct unary_match
{
    static_assert(Kind >= Expr::FirstUnary, "Unary expressions must be NonNullary!");

    PatternTy pattern;

    unary_match(const PatternTy& pattern) : pattern(pattern) {}

    template<typename InputTy>
    bool match(const ExprRef<InputTy>& expr)
    {
        if (expr->getKind() != Kind) {
            return false;
        }

        if (NonNullaryExpr* e = llvm::dyn_cast<NonNullaryExpr>(expr.get())) {
            return pattern.match(e->getOperand(0));
        }

        return false;
    }
};

#define UNARY_MATCHER(KIND)                                                     \
template<typename PatternTy>                                                    \
inline unary_match<PatternTy, Expr::KIND> m_##KIND(const PatternTy& pattern) {  \
    return unary_match<PatternTy, Expr::KIND>(pattern);                         \
}

UNARY_MATCHER(Not)
UNARY_MATCHER(ZExt)
UNARY_MATCHER(SExt)

#undef UNARY_MATCHER

//===------------------- Matcher for binary expressions -------------------===//
//============================================================================//

/// Helper class for matching binary expressions.
/// The pattern is always evaluated left to right, regardless of commutativity.
template<typename LTy, typename RTy, Expr::ExprKind Kind, bool Commutable = false>
struct binary_match
{
    static_assert(Kind >= Expr::FirstUnary, "Binary expressions must be NonNullary!");

    LTy left;
    RTy right;

    binary_match(const LTy& left, const RTy& right) : left(left), right(right) {}

    template<typename InputTy>
    bool match(const ExprRef<InputTy>& expr)
    {
        if (expr->getKind() != Kind) {
            return false;
        }

        if (NonNullaryExpr* e = llvm::dyn_cast<NonNullaryExpr>(expr.get())) {
            if (e->getNumOperands() != 2) {
                return false;
            }

            return (left.match(e->getOperand(0)) && right.match(e->getOperand(1))) ||
                (Commutable && left.match(e->getOperand(1)) && right.match(e->getOperand(0)));
        }

        return false;
    }
};

#define BINARY_MATCHER(KIND)                                            \
template<typename LTy, typename RTy>                                    \
inline binary_match<LTy, RTy, Expr::KIND> m_##KIND(                     \
    const LTy& left, const RTy& right                                   \
) {                                                                     \
    return binary_match<LTy, RTy, Expr::KIND, false>(left, right);      \
}

#define BINARY_MATCHER_COMMUTATIVE(KIND)                                \
template<typename LTy, typename RTy>                                    \
inline binary_match<LTy, RTy, Expr::KIND, true> m_##KIND(               \
    const LTy& left, const RTy& right                                   \
) {                                                                     \
    return binary_match<LTy, RTy, Expr::KIND, true>(left, right);       \
}

BINARY_MATCHER_COMMUTATIVE(Add)
BINARY_MATCHER(Sub)
BINARY_MATCHER_COMMUTATIVE(Mul)
BINARY_MATCHER(BvSDiv)
BINARY_MATCHER(BvUDiv)
BINARY_MATCHER(BvSRem)
BINARY_MATCHER(BvURem)
BINARY_MATCHER(Shl)
BINARY_MATCHER(LShr)
BINARY_MATCHER(AShr)
BINARY_MATCHER_COMMUTATIVE(BvAnd)
BINARY_MATCHER_COMMUTATIVE(BvOr)
BINARY_MATCHER_COMMUTATIVE(BvXor)
BINARY_MATCHER_COMMUTATIVE(And)
BINARY_MATCHER_COMMUTATIVE(Or)
BINARY_MATCHER_COMMUTATIVE(Xor)
BINARY_MATCHER_COMMUTATIVE(Eq)
BINARY_MATCHER_COMMUTATIVE(NotEq)
BINARY_MATCHER(BvSLt)
BINARY_MATCHER(BvSLtEq)
BINARY_MATCHER(BvSGt)
BINARY_MATCHER(BvSGtEq)
BINARY_MATCHER(BvULt)
BINARY_MATCHER(BvULtEq)
BINARY_MATCHER(BvUGt)
BINARY_MATCHER(BvUGtEq)

#undef BINARY_MATCHER
#undef BINARY_MATCHER_COMMUTATIVE

template<typename CondTy, typename LTy, typename RTy>
struct select_expr_match
{
    CondTy cond;
    LTy then;
    RTy elze;

    select_expr_match(const CondTy& cond, const LTy& then, const RTy& elze) : cond(cond), then(then), elze(elze) {}

    template<typename InputTy>
    bool match(const ExprRef<InputTy>& expr)
    {
        if (auto select = llvm::dyn_cast<SelectExpr>(expr.get())) {
            return cond.match(select->getCondition())
                && then.match(select->getThen())
                && elze.match(select->getElse());
        }

        return false;
    }
};

template<typename CondTy, typename LTy, typename RTy>
select_expr_match<CondTy, LTy, RTy> m_Select(const CondTy& cond, const LTy& left, const RTy& right)
{
    return select_expr_match<CondTy, LTy, RTy>(cond, left, right);
}


//===------------------ Matcher for multiary expressions ------------------===//
//============================================================================//

/// Helper for matching multiary expressions
template<size_t NumOps, Expr::ExprKind Kind, bool Commutable, typename... Ts>
struct multiary_match_precise
{
    std::tuple<Ts...> patterns;

    multiary_match_precise(const std::tuple<Ts...>& patterns)
        : patterns(patterns)
    {}

    template<size_t N>
    bool subpattern_match_unordered(NonNullaryExpr* expr, std::bitset<NumOps> bs)
    {
        if constexpr (N < NumOps) {
            auto& pattern = std::get<N>(patterns);

            // The pattern could be matched in any order.
            // To do this, we traverse the operands of the input expression
            // and try to match the pattern. If the pattern is matched,
            // we set the corresponding bit in the bitset, so further
            // patterns will not match on this one.
            bool matched = false;
            for (size_t i = 0; i < NumOps; ++i) {
                if (!bs[i] && pattern.match(expr->getOperand(i))) {
                    matched = true;
                    bs.set(i);
                    break;
                }
            }

            // No valid matches were found for this pattern
            if (!matched) {
                return false;
            }

            return subpattern_match_unordered<N + 1>(expr, bs);
        }

        return true;
    }

    template<size_t N>
    bool subpattern_match(NonNullaryExpr* expr)
    {
        if constexpr (N < NumOps) {
            auto& pattern = std::get<N>(patterns);

            // The order matters here (non-commutable): we just need to
            // check if the ith operand matches the ith pattern.
            bool matched = pattern.match(expr->getOperand(N));

            if (!matched) {
                return false;
            }

            return subpattern_match<N + 1>(expr);
        }

        return true;
    }

    template<typename InputTy>
    bool match(const ExprRef<InputTy>& expr)
    {
        if (expr->getKind() != Kind) {
            return false;
        }

        NonNullaryExpr* e = llvm::cast<NonNullaryExpr>(expr.get());
        if (e->getNumOperands() != NumOps) {
            return false;
        }

        // Check each pattern separately.
        if constexpr (Commutable) {
            std::bitset<NumOps> bs;
            return subpattern_match_unordered<0>(e, bs);
        }
        
        return subpattern_match<0>(e);
    }
};

template<size_t NumPatterns, Expr::ExprKind Kind, typename... Ts>
struct multiary_match
{
    ExprVector& unmatched;
    std::tuple<Ts...> patterns;

    multiary_match(ExprVector& unmatched, const std::tuple<Ts...>& patterns)
        : patterns(patterns)
    {}

    template<size_t N>
    bool subpattern_match_unordered(NonNullaryExpr* expr, std::vector<bool>& bs)
    {
        if constexpr (N < NumPatterns) {
            auto& pattern = std::get<N>(patterns);

            bool matched = false;
            for (size_t i = 0; i < NumPatterns; ++i) {
                if (!bs[i] && pattern.match(expr->getOperand(i))) {
                    matched = true;
                    bs[i] = true;
                    break;
                }
            }

            // No valid matches were found for this pattern
            if (!matched) {
                return false;
            }

            return subpattern_match_unordered<N + 1>(expr, bs);
        }

        return true;
    }

    template<typename InputTy>
    bool match(const ExprRef<InputTy>& expr)
    {
        if (expr->getKind() != Kind) {
            return false;
        }

        NonNullaryExpr* e = llvm::cast<NonNullaryExpr>(expr.get());
        if (e->getNumOperands() < NumPatterns) {
            return false;
        }

        // Check each pattern separately.
        std::vector<bool> bs(e->getNumOperands(), false);
        bool matched = subpattern_match_unordered<0>(e, bs);

        // Collect all the unmatched operands
        for (size_t i = 0; i < bs.size(); ++i) {
            unmatched.push_back(e->getOperand(i));
        }

        return matched;
    }
};

template<typename FirstTy, typename SecondTy, typename... Ts>
multiary_match_precise<sizeof...(Ts) + 2, Expr::And, false, FirstTy, SecondTy, Ts...>
m_Ordered_And(const FirstTy& first, const SecondTy& second, const Ts&... patterns)
{
    return multiary_match_precise<
        sizeof...(Ts) + 2,
        Expr::And, false,
        FirstTy, SecondTy, Ts...
    >(std::make_tuple(first, second, patterns...));
}

template<typename FirstTy, typename SecondTy, typename... Ts>
multiary_match_precise<sizeof...(Ts) + 2, Expr::And, true, FirstTy, SecondTy, Ts...>
m_And(const FirstTy& first, const SecondTy& second, const Ts&... patterns)
{
    return multiary_match_precise<
        sizeof...(Ts) + 2,
        Expr::And, true,
        FirstTy, SecondTy, Ts...
    >(std::make_tuple(first, second, patterns...));
}

} // end namespace PatternMatch
} // end namespace gazer

#endif