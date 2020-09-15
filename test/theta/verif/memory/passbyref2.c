// REQUIRES: memory.burstall
// RUN: %theta --domain PRED_CART --refinement NWT_IT_WP "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));

void sumprod(int a, int b, int *sum, int *prod)
{
    *sum = a + b;
    *prod = a * b;
}

int main(void)
{
    int a = __VERIFIER_nondet_int();
    int b = a + 1;

    int sum, prod;
    sumprod(a, b, &sum, &prod);

    if (a != 0 && b != 0 && prod == 0) {
        __VERIFIER_error();
    }

    return 0;
}
