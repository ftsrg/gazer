// RUN: %bmc -bound 1 -no-inline-globals "%s" | FileCheck "%s"
// RUN: %bmc -memory=simple -bound 1 -no-inline-globals "%s" | FileCheck "%s"

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}
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
