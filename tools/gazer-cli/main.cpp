#include "gazer/Core/Utils/ExprBuilder.h"
#include "gazer/Z3Solver/Z3Solver.h"

#include <llvm/Support/raw_ostream.h>

#include <iostream>

using namespace gazer;

int main(void)
{
    z3::context ctx;
    z3::solver solver(ctx);

    z3::sort fltSort(ctx, Z3_mk_fpa_sort_32(ctx));
    z3::expr x = ctx.constant("x", fltSort);
    z3::expr y = ctx.constant("y", z3::sort(ctx, Z3_mk_bv_sort(ctx, 64)));

    auto isNaN  = z3::expr(ctx, Z3_mk_fpa_is_nan(ctx, x));
    auto toIEEE = z3::expr(ctx, Z3_mk_fpa_to_ieee_bv(ctx, x));
    auto eq = z3::eq(y, toIEEE);

    solver.add(isNaN && eq);
    std::cerr << solver << "\n";

    auto res = solver.check();

    if (res == z3::sat) {
        std::cerr << "----------\n";
        std::cerr << solver.get_model() << "\n";
    }
}
