// RUN: %bmc -bound 1 -inline-level=all -trace "%s" | FileCheck "%s"

// CHECK: Verification FAILED

// This test caused trace generation to crash due to an invalid null pointer.

int c;
void __VERIFIER_error();
int a();
int b();
int d() {
  b();
  __VERIFIER_error();
  return 0;
}
int b() { return c; }
int main() {
  int e = a();
  c = 0;
  if (e)
    c = 7;
  d();
}
