// RUN: %theta -memory=havoc "%s" -math-int | FileCheck "%s"
// RUN: %theta --domain EXPL --refinement UNSAT_CORE "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL

int __VERIFIER_nondet_int(void);

int main(void) {
    int x = __VERIFIER_nondet_int();
    if (x > 0) {
        return 0;
    }

    return 1;
}