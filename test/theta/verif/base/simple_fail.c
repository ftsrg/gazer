// RUN: %theta -memory=havoc "%s" -math-int | FileCheck "%s"

// CHECK: Verification FAILED
#include <assert.h>

int main(void)
{
    int a;
    a = 1;
    a = -1;

    assert(a != -1);
}
