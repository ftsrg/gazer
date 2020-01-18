// RUN: %bmc -bound 10 "%s" | FileCheck "%s"

// CHECK: Verification FAILED
#include <assert.h>

extern int __VERIFIER_nondet_int(void);

int main(void)
{
    int i = 0;
    int n = __VERIFIER_nondet_int();
    int x = __VERIFIER_nondet_int();
    int sum = 0;
    int prod = 0;

    for (int i = 0; i < n; ++i) {
        for (int j = i; j < n; ++j) {
            if (j % x == 0) {
                goto err;
            }
        }
    }

    goto ok;

err:
    assert(0);

ok:
    return 0;
}