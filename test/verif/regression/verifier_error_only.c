// RUN: %bmc -bound 1 -trace -memory=flat "%s" | FileCheck "%s"

// CHECK: Verification FAILED

void __VERIFIER_error();
int main() { __VERIFIER_error(); }