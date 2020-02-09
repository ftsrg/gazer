// RUN: %bmc "%s" | FileCheck "%s"

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}

// Due to an erroneous encoding of input assignments during BMC inlining,
// this test yielded a false positive result.

int c, d, e, f, h, i;
void __VERIFIER_error();
int a();
int b();
int g() {
  b(0, e, f, 0, 0);
  return 0;
}
int j() {
  a(h, i);
  return 0;
}
int k() { return 0; }
int a(l, m) {
  k();
  b(0, e, f, 0, 0);
  k();
  return 0;
}
int main() {
  c = 1;
  j();
}
int b(int n, int o, int p, int q, int r) {
  if (c == d)
    __VERIFIER_error();
  return 0;
}
