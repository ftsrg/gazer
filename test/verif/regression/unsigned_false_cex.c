// RUN: %bmc -inline -inline-globals -no-optimize -math-int -trace -test-harness %t1.ll "%s"
// RUN: %check-cex "%s" "%t1.ll" "%errors" | FileCheck "%s"

// RUN: %bmc -inline -inline-globals -math-int -trace -test-harness %t3.ll "%s"
// RUN: %check-cex "%s" "%t3.ll" "%errors" | FileCheck "%s"

// CHECK: __VERIFIER_error executed

// Due to an erroneous encoding of unsigned comparisons, we have produced some
// false-positive counterexamples in -math-int mode. This test should evaluate
// as a failure and the resulting counterexample is verified through lli.

int a;
int d = 5;
void __VERIFIER_error();
int b();
int a17 = 1;

int c = 5;
int e() {
  if (d && ((c == 1)))
    ;
  else if (((!a && (((!a17) || (c && 0))))))
    d = 14;
  else if ((c) && d)
    a17 = 0;
  if (1 && d == 14)
    __VERIFIER_error();
  return 2;
}
int main() {
  while (1) {
    int f = b();
    if (f != 5 && f != 6)
      return 2;
    e();
  }
}
