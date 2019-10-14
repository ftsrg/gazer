// RUN: %bmc -bound 1 "%s" | FileCheck "%s"

// CHECK: Verification FAILED
#include <assert.h>

int main(void)
{
    int a;
    a = 1;
    a = -1;

    assert(a != -1);
}
