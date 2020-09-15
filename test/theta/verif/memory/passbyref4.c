// REQUIRES: memory.burstall
// RUN: %theta --domain PRED_CART --refinement NWT_IT_WP "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));
void __VERIFIER_assume(int expression);

void make_symbolic(void* ptr);

int main(void)
{
    int a, b;
    make_symbolic(&a);
    make_symbolic(&b);

    if (a == 0) {
        b = a + 1;
    } else {
        b = a + 2;
    }

    if (a > b) {
        __VERIFIER_error();
    }

    return 0;
}