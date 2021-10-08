// REQUIRES: memory.burstall
// RUN: %theta --domain PRED_CART --refinement NWT_IT_WP "%s" | FileCheck "%s"

// CHECK: Verification FAILED

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));

void make_symbolic(int* ptr);

int main(void)
{
    int a = __VERIFIER_nondet_int();
    make_symbolic(&a);

    if (a == 0) {
        __VERIFIER_error();
    }

    return 0;
}
