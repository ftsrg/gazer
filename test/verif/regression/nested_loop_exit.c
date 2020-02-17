// RUN: %bmc "%s" | FileCheck "%s"

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}

int main() {
  int a;
  while (1) {
    int b, c;
    while (1) {
      if (c)
        goto d;
      if (b)
        while (1)
          ;
    }
  d:
    if (a)
      goto e;
  }
e:;
}
