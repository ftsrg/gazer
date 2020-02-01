// RUN: %bmc -bound 1 -memory=flat "%s" | FileCheck "%s" --check-prefix=MODIFY

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}

// MODIFY: Verification FAILED

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
    x.a = a;

    make_symbolic(&x.b);

    if (x.a != a) {
        __VERIFIER_error();
    }

    return 0;
}