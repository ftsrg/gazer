// RUN: %theta -checks=signed-overflow "%s" | FileCheck "%s"

// CHECK: Verification FAILED

int __VERIFIER_nondet_int();

int main(void)
{
    int x = __VERIFIER_nondet_int();
    int y = __VERIFIER_nondet_int();

    // CHECK: Signed integer overflow in {{.*}}overflow_simple.c at line [[# @LINE + 1]] column 14
    return x + y;
}
