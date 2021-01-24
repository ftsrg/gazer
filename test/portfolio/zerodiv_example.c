// RUN: %portfolio -c "%S"/SVComp_configuration.yml -l minimal -t "%s" -o /tmp | FileCheck "%s"

// CHECK: Result of bmc-inline: Verification FAILED.
// CHECK-NEXT: Result of bmc-inline-test-harness: Test harness SUCCESSFUL WITNESS
// CHECK: Final result of portfolio: Verification FAILED.
#include <stdio.h>

extern int ioread32(void);

int main(void) {
    int k = ioread32();
    int i = 0;
    int j = k + 5;
    while (i < 3) {
        i = i + 1;
        j = j + 3;
    }

    k = k / (i - j);

    printf("%d\n", k);

    return 0;
}
