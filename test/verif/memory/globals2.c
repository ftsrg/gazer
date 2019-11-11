// XFAIL: memory
// RUN: %bmc -bound 1 "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL
int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));


int b = 1;

int main(void)
{
    int a = __VERIFIER_nondet_int();

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