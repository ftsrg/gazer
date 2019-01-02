#include "gazer/LLVM/Instrumentation/Check.h"

namespace gazer
{
    namespace checks
    {
        /**
         * Check for assertion violations within the program.
         */
        Check* CreateAssertionFailCheck();

        /**
         * This check fails if a division instruction is reachable
         * while the second operand's value is 0.
         */
        Check* CreateDivisionByZeroCheck();

        /**
         * This check fails if a signed integer operation results
         * in an over- or underflow.
         */
        Check* CreateSignedIntegerOverflowCheck();
    
    } // end namespace checks

} // end namespace gazer