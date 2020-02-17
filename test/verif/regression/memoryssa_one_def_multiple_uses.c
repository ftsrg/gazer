// RUN: %bmc -memory=flat "%s" | FileCheck "%s"

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}

int a() { return 0; }
int main() {
  while (1) {
    a();
    a();
  }
}
