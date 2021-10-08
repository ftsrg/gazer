// RUN: %theta --domain PRED_CART --refinement NWT_IT_WP -no-inline-globals "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL
#include <limits.h>

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));
void __VERIFIER_assume(int expression);

int b = 1;

int main(void)
{
    int a = __VERIFIER_nondet_int();
    __VERIFIER_assume(a < INT_MAX - 1);

    if (a == 0) {
        b = a + 1;
    } else {
        b = a + 2;
    }

    if (a > b) {
        __VERIFIER_error();
    }

    return 0;
}