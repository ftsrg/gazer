// XFAIL: memory
// RUN: %bmc -bound 1 "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL
int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));

int b = 1;
int c = 2;

int main(void)
{
    int a = __VERIFIER_nondet_int();
    int* ptr;

    if (a == 0) {
        ptr = &b;
    } else {
        ptr = &c;
    }

    if (*ptr > a) {
        __VERIFIER_error();
    }

    return 0;
}