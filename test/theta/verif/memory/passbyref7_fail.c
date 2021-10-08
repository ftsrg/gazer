// REQUIRES: memory.burstall
// RUN: %theta --domain PRED_CART --refinement NWT_IT_WP "%s" | FileCheck "%s"

// CHECK: Verification FAILED

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));

void sumprod(int a, int b, int *sum, int *prod)
{
    *sum = a + b;
    *prod = a * b;
}

int main(void)
{
    int a = 5;
    int b = 5;

    int prod;
    sumprod(a, b, &prod, &prod);

    if (prod == 25) {
        __VERIFIER_error();
    }

    return 0;
}
