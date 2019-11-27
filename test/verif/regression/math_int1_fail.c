// RUN: %bmc -bound 1 -math-int "%s" | FileCheck "%s"

// CHECK: Verification FAILED

int a, b = 5;
void __VERIFIER_error();
int main() {
  while (1) {
    if (b == 5 || b == 6)
      b = a = 4;
    if (a)
      __VERIFIER_error();
  }
}
