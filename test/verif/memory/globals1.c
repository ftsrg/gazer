// XFAIL: memory
// RUN: %bmc -bound 1 -math-int "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL
#include <limits.h>

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));
void __VERIFIER_assume(int);

int a = 0, b = 1;

int main(void)
{
    a = __VERIFIER_nondet_int();
    __VERIFIER_assume(a < INT_MAX - 1);
    b = a + 1;

    if (a > b) {
        __VERIFIER_error();
    }

    return 0;
}