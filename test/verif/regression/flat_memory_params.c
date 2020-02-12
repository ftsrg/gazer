// RUN: %bmc -bound 1 -memory=flat -checks="assertion-fail" "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL

// This test failed because the SSA-based memory model inserted all memory objects
// as loop procedure inputs instead of just inserting the ones which were actually
// used by the loop. Furthermore, this test also produced a crash in the globals
// variable inling pass due to its incorrect handling of constant expressions.

void f(int i);

struct b *c;
struct b {
  int d;
} e() {
  f(c->d);
  return *c;
}

int main() {
  int a;
  for (; a;)
    ;
  e();
}
