// This test failed if gazer-bmc was invoked without the "-inline=all" flag.
// The underlying issue was that the translation process did not handle PHI nodes correctly when jumping out of loops.

// RUN: %bmc -bound 10 -inline-level=all "%s" | FileCheck "%s"
// RUN: %bmc -bound 10 "%s" | FileCheck "%s"

// CHECK: Verification {{(SUCCESSFUL|BOUND REACHED)}}

float b = 16.f;
void __VERIFIER_error();
void a(c) {
    if (!c)
        __VERIFIER_error();
}
int main() {
    for (;;) {
        b = 3.f * b / 4.f;
        a(12.f);
        a(b);
    }
}
