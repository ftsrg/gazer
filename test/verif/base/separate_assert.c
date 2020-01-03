// RUN: %bmc "%s" | FileCheck "%s"

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}

void a(b) {
  if (!b)
    __VERIFIER_error();
}
main() { a(70789); }
