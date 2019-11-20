// XFAIL: memory
// RUN: %bmc -bound 1 "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));

void sumprod(int* a, int* b, int *sum, int *prod)
{
    *sum = *a + *b;
    *prod = *a * *b;
}

int main(void)
{
    int a = __VERIFIER_nondet_int();
    int b = a + 1;
    int c = __VERIFIER_nondet_int();

    int* p = a == 0 ? &c : &a;

    int sum, prod;
    sumprod(p, &b, &sum, &prod);

    if (a != 0 && b != 0 && prod == 0) {
        __VERIFIER_error();
    }

    return 0;
}
