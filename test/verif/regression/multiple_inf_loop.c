// RUN: %bmc "%s" | FileCheck "%s"

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}

// This test failed due to an erroneous handling of LLVM's loop information
// in the ModuleToAutomataPass

long a;
int c() {
  int b;
  for (; b;)
    a = b;
  return a;
}
int main() {
  c();
  for (;;)
    c();
}
