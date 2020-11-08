// RUN: %bmc -checks=signed-overflow "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL

void a() __attribute__((__noreturn__));
int b() {
  int c;
  if (c - 4L)
    ;
  a();
}
int main() { b(); }
