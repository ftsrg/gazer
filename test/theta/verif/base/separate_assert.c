// RUN: %theta -memory=havoc "%s" -math-int  | FileCheck "%s"

// CHECK: Verification SUCCESSFUL

void a(b) {
  if (!b)
    __VERIFIER_error();
}
main() { a(70789); }
