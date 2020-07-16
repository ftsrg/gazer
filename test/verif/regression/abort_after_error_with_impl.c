// RUN: %bmc "%s" | FileCheck "%s"

// CHECK: Verification FAILED

extern void abort(void);
void __VERIFIER_error() {
    // empty
}

extern int __VERIFIER_nondet_int();

int main() {
    int x = __VERIFIER_nondet_int();
    if (x) {
        __VERIFIER_error();
        abort();
    }

    return 0;
}