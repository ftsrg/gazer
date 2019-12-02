// RUN: %bmc -bound 10 "%s" | FileCheck "%s"

// CHECK: Verification FAILED
#include <assert.h>

extern int __VERIFIER_nondet_int(void);

int main(void)
{
    int i = 0;
    int n = __VERIFIER_nondet_int();
    int sum = 0;
    int prod = 0;

    while (i < n) {
        int j = 0;
        int x = 0;
        while (j < n) {
            x = (x + 1) * j;
            ++j;
        }
        sum = sum + i + x;
        prod = (prod + i) * x;
        ++i;
    }

    assert(sum != 0);

    return 0;
}