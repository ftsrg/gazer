// RUN: %bmc "%s" | FileCheck "%s"

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}

// Due to an erroneous expression rewrite transformation, this
// test has produced an invalid counterexample. 

int b, c, d, e, g;
void __VERIFIER_error();
int a();
int f() {
  a(f, d, e, 0, 0);
  return 0;
}
int h() {
  a(g, d, e, 0, 0);
  return 0;
}
int main() {
  b = 1;
  h();
}
int a(int i, int j, int k, int l, int m) {
  if (b == c)
    __VERIFIER_error();
  return 0;
}
