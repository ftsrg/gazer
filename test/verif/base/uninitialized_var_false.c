// RUN: %gazer bmc -bound 1 -no-optimize "%s" | FileCheck "%s"
// RUN: %gazer bmc -bound 1 "%s" | FileCheck "%s"

// CHECK: Verification FAILED

void __VERIFIER_error(void) __attribute__((noreturn));

int main(void)
{
    int x;

    if (x == 1) {
        __VERIFIER_error();
    }

    return 0;
}