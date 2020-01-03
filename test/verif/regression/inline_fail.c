// RUN: %bmc -bound 10 -math-int "%s" | FileCheck "%s"
// RUN: %bmc -bound 10 -math-int "%s" | FileCheck "%s"

// CHECK: Verification FAILED

int a, b;

int c(f) {
  if (f) {
    b = 3;
    return 3;
  }
  if (b)
    a = e();
}

int e() {
  if (a)
    __VERIFIER_error();
}

int main(void) {
  while (1) {
    int d;
    c(d);
  }
}
