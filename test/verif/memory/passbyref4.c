// RUN: %bmc -bound 1 "%s" | FileCheck "%s"

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}
#include <limits.h>

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));
void __VERIFIER_assume(int expression);

void make_symbolic(void* ptr);

int main(void)
{
    int a, b;
    make_symbolic(&a);
    make_symbolic(&b);
    
    __VERIFIER_assume(a < INT_MAX - 2);
    __VERIFIER_assume(b > 0);
    
    if (a != 0) {
        b = a + 2;
    }

    if (a > b) {
        __VERIFIER_error();
    }

    return 0;
}