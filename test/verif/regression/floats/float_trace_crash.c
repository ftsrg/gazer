// RUN: %bmc -trace "%s" | FileCheck "%s"
// CHECK: Verification FAILED

int b;
void __VERIFIER_error();
float a();
int main() {
    float c = a();
    b = c != c; // CHECK: b := 0
    if (!b)
        __VERIFIER_error();
}
