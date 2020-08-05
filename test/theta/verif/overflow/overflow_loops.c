// RUN: %theta -checks=signed-overflow "%s" | FileCheck "%s"

// CHECK: Verification FAILED
// CHECK: Signed integer overflow

int __VERIFIER_nondet_int();

int main(void)
{
    int x = 1;
    int y = 1;

    while (1) {
        x = x * 2 * __VERIFIER_nondet_int();
        y = y * 3 * __VERIFIER_nondet_int();
        if (x == 0) {
            goto EXIT;
        }
    }

EXIT:
    return 0;
}
