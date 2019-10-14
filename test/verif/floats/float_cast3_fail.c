// RUN: %bmc -bound 1 "%s" | FileCheck "%s"

// CHECK: Verification FAILED
#include <stdint.h>

extern void __VERIFIER_error(void);
extern float __VERIFIER_nondet_float(void);
extern double __VERIFIER_nondet_double(void);
uint32_t __VERIFIER_nondet_uint32(void);

int main(void)
{
    float x = __VERIFIER_nondet_float();
    float y = __VERIFIER_nondet_float();
    uint32_t t = __VERIFIER_nondet_uint32();

    float mul1 = x * x * y + t;
    float mul2 = x * (x * y) + t;

    if (mul1 != mul2) {
        __VERIFIER_error();
    }

    return 0;
}
