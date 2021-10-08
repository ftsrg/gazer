// REQUIRES: memory.structs
// RUN: %theta --domain PRED_CART --refinement NWT_IT_WP "%s" | FileCheck "%s"

// CHECK: Verification FAILED

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));

void make_symbolic(void* ptr);

typedef struct X
{
    int a;
    int b;
} X;

int main(void)
{
    X x;
    int a = __VERIFIER_nondet_int();
    make_symbolic(&x);

    if (x.a != a) {
        __VERIFIER_error();
    }

    return 0;
}