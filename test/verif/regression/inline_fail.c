// RUN: %bmc -inline -bound 10 -math-int "%s" | FileCheck "%s"
// RUN: %bmc -bound 10 -math-int "%s" | FileCheck "%s"

// CHECK: Verification FAILED

a, b;
c(f) {
  if (f) {
    b = 3;
    return 3;
  }
  if (b)
    a = e();
}
e() {
  if (a)
    __VERIFIER_error();
}
main() {
  while (1) {
    int d;
    c(d);
  }
}
