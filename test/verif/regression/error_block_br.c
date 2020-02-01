// RUN: %bmc -bound 1 "%s" | FileCheck "%s"

// CHECK: Verification FAILED

// This test makes sure that even if a unifyFunctionExitNodes pass was run,
// the error block detection in ModuleToAutomata works well.

void __VERIFIER_error();
void exit();

int a;

int b(c) {
  if (a)
    exit(0);
  __VERIFIER_error();
  return 2;
}

int main() { int c = b(c); }
