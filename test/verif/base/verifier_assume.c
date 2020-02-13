// RUN: %bmc "%s" | FileCheck "%s"

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}

int __VERIFIER_nondet_int(void);
void __VERIFIER_assume(int);
void __VERIFIER_error(void) __attribute__((__noreturn__));

int main()
{
    int y = __VERIFIER_nondet_int();

    __VERIFIER_assume(y != 0);

    if (!y) {
        __VERIFIER_error();
    }

    return 0;
}
