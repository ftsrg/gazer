// RUN: %theta -memory=havoc "%s" -math-int  | FileCheck "%s"
// RUN: %theta --domain EXPL --refinement UNSAT_CORE "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL

void a(b) {
  if (!b)
    __VERIFIER_error();
}
main() { a(70789); }
