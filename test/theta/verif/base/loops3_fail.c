// RUN: %theta -no-optimize -memory=havoc -math-int "%s" | FileCheck "%s"
// RUN: %theta --domain EXPL --refinement UNSAT_CORE "%s" | FileCheck "%s"

// CHECK: Verification FAILED
#include <assert.h>

extern int __VERIFIER_nondet_int(void);

int main(void)
{
    int i = 0;
    int n1 = __VERIFIER_nondet_int();
    int n2 = __VERIFIER_nondet_int();
    int sum = 0;

    while (i < n1) {
        sum = sum + i;
        ++i;
    }

    while (i < n2) {
        sum = sum * i;
        ++i;
    }

    assert(sum != 0);

    return 0;
}