// RUN: %bmc -bound 1 -inline=all -trace "%s" | FileCheck "%s"

// CHECK: Verification FAILED

// Invalid handling of removed global variables caused this test to crash during trace generation. 

int b;
void __VERIFIER_error();
int a();
int main() {
  int c = a();
  b = 0;
  if (c)
    b = 1;
  __VERIFIER_error();
}
