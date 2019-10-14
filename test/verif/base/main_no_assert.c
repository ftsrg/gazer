// RUN: %bmc -bound 10 "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL

int __VERIFIER_nondet_int(void);

int main(void) {
    int x = __VERIFIER_nondet_int();
    if (x > 0) {
        return 0;
    }

    return 1;
}