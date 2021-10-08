// RUN: %theta --domain PRED_CART --refinement NWT_IT_WP "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));

void make_symbolic(void* ptr);

int a, b;

int main(void)
{
    int* pA = &a;
    int* pB = &b;

    a = __VERIFIER_nondet_int();
    b = a;

    if (*pA != *pB) {
        __VERIFIER_error();
    }

    return 0;
}