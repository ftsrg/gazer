// RUN: %bmc -bound 10 "%s" | FileCheck "%s"

// CHECK: Verification FAILED
#include <assert.h>

unsigned __VERIFIER_nondet_uint(void);

int main(void)
{
    unsigned i = 0;
    unsigned c = __VERIFIER_nondet_uint();

    unsigned x = 1;
    while (i < c) {
        unsigned y = __VERIFIER_nondet_uint();
        x = x + y;
        ++i;
    }

    assert(x != 0);

    return 0;
}
