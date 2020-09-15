// RUN: %theta --domain PRED_CART --refinement NWT_IT_WP -no-inline-globals "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL
int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));

int a = 0, b = 1;

int main(void)
{
    a = __VERIFIER_nondet_int();
    b = a + 1;

    if (a > b) {
        __VERIFIER_error();
    }

    return 0;
}