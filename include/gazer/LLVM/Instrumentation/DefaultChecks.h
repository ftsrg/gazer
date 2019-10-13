#include "gazer/LLVM/Instrumentation/Check.h"

namespace gazer::checks
{
    /// Check for assertion violations within the program.
    Check* CreateAssertionFailCheck();

    /// This check fails if a division instruction is reachable
    /// with its second operand's value being 0.
    Check* CreateDivisionByZeroCheck();

    /// This check fails if a signed integer operation results
    /// in an over- or underflow.
    Check* CreateSignedIntegerOverflowCheck();

} // end namespace gazer::checks