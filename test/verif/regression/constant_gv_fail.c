// RUN: %bmc "%s" | FileCheck "%s"

// CHECK: Verification FAILED

#include <assert.h>
int main(void) { assert(0); }

