#include "gazer/LLVM/Instrumentation/Check.h"

namespace gazer
{

namespace checks
{

Check* CreateAssertionFailCheck();
Check* CreateDivisionByZeroCheck();
Check* CreateIntegerOverflowCheck();

} // end namespace checks

} // end namespace gazer