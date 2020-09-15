// REQUIRES: memory.burstall
// RUN: %theta --domain PRED_CART --refinement NWT_IT_WP "%s" | FileCheck "%s"

// CHECK: Verification FAILED
int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));

int main(void)
{
    int x[5];

    for (int i = 0; i < 5; ++i) {
        x[i] = i;
    }

    if (x[0] == 0) {
        __VERIFIER_error();
    }

    return 0;
}