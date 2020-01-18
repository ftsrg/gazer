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
        if (i % 2) {
            goto mid;
        }
restart:
        for (int j = i; j < n; ++j) {
            if (j % x == 0) {
                mid:
                i = i - 1;
                goto restart;
            }
        }
    }

    assert(sum != 0);

    return 0;
}