// RUN: %bmc -bound 1 -Wno-int-conversion "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL

struct {
  long d;
  long e;
} typedef f;
struct {
} typedef g;
typedef struct h i;
f j(k) {
  f l;
  return l;
}
long m(n, o) {
  f frequency;
  j(&frequency);
  return 0;
}
i *p;
int main() {
  g devobj;
  m(&devobj, p);
}
