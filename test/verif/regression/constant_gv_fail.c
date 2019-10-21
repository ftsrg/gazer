// RUN: %bmc -inline -inline-globals "%s" | FileCheck "%s"

// CHECK: Verification FAILED

#include <assert.h>
int main(void) { assert(0); }

