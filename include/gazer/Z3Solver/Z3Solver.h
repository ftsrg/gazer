#ifndef _GAZER_CORE_Z3SOLVER_H
#define _GAZER_CORE_Z3SOLVER_H

#include "gazer/Core/Solver/Solver.h"

namespace z3 {
    class context;
    class model;
    class expr;
} // end namespace z3

namespace gazer
{

class Z3SolverFactory : public SolverFactory
{
public:
    Z3SolverFactory(bool cache = true)
        : mCache(cache)
    {}

    virtual std::unique_ptr<Solver> createSolver(GazerContext& context) override;

private:
    bool mCache;
};

/// Utility function which transforms an arbitrary Z3 bitvector into LLVM's APInt.
llvm::APInt z3_bv_to_apint(z3::context& context, z3::model& model, const z3::expr& expr);

} // end namespace gazer

#endif
