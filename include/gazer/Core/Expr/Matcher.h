#ifndef _GAZER_CORE_EXPR_MATCHER_H
#define _GAZER_CORE_EXPR_MATCHER_H

#include <llvm/Support/Casting.h>

/**
 * This file describes utilities for pattern matching and rewriting expressions.
 */

namespace gazer { namespace expr {

template<typename ExprTy>
struct expr_match {
    template<typename InputTy>
    bool match(const std::shared_ptr<InputTy>& ptr) {
        return llvm::isa<ExprTy>(ptr.get());
    }
};

template<typename ExprTy>
struct bind_ty
{

}

/**
 * This namespace holds pattern matching utilities for expressions.
 */
namespace m
{

}

}}

#endif
