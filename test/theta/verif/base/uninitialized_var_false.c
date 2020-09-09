// RUN: %theta -no-optimize -memory=havoc "%s" -math-int | FileCheck "%s"
// RUN: %theta -memory=havoc "%s" -math-int  | FileCheck "%s"
// RUN: %theta --domain EXPL --refinement UNSAT_CORE "%s" | FileCheck "%s"

// CHECK: Verification FAILED

// This test checks that the enabled LLVM optimizations do not
// strip away relevant code under an undefined value.
void __VERIFIER_error(void) __attribute__((noreturn));

int main(void)
{
    int x;

    if (x == 1) {
        __VERIFIER_error();
    }

    return 0;
}