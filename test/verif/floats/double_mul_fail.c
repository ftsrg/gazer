// RUN: %bmc -bound 1 "%s" | FileCheck "%s"

// CHECK: Verification FAILED

extern void __VERIFIER_error(void);
extern double __VERIFIER_nondet_double(void);

int main(void)
{
    double x = __VERIFIER_nondet_double();
    double y = __VERIFIER_nondet_double();

    double mul1 = x * x * y;
    double mul2 = x * (x * y);

    if (mul1 != mul2) {
        __VERIFIER_error();
    }

    return 0;
}
