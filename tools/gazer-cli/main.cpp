#include "gazer/Core/Expr/ExprBuilder.h"
#include "gazer/Z3Solver/Z3Solver.h"

#include <llvm/Support/raw_ostream.h>

#include <iostream>

using namespace gazer;

int main(void)
{
    llvm::APInt five{32, static_cast<uint64_t>(-5)};
    llvm::errs() << five << "\n";
    llvm::APInt two{32, 2};
    llvm::errs() << two << "\n";

    llvm::errs() << five.ult(two) << "\n";
    llvm::errs() << five.slt(two) << "\n";

    llvm::errs() << llvm::APInt::getMaxValue(8).getSExtValue() << "\n";
}
