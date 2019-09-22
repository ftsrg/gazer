// RUN: %gazer bmc -bound 1 "%s" | FileCheck "%s"

// CHECK: Verification FAILED
#include <assert.h>
#include <stdint.h>

float __VERIFIER_nondet_float(void);
uint32_t __VERIFIER_nondet_uint32(void);

int main(void)
{
    double x = __VERIFIER_nondet_float();
    uint32_t t = __VERIFIER_nondet_uint32();

    double f = x * ((double) 1500) * ((float) t);

    assert(f != 0);

    return 0;
}
