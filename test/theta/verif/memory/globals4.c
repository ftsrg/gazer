// RUN: %theta --domain PRED_CART --refinement NWT_IT_WP -no-inline-globals "%s" | FileCheck "%s"

// CHECK: Verification SUCCESSFUL

int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((__noreturn__));

int a = 1;
int b = 1;
int c = 3;

int main(void)
{
    int input = 1;
    while (input != 0) {
        input = __VERIFIER_nondet_int();
        if (input == 1) {
            a = a + 1;
        }
    }

    if (a < b) {
        __VERIFIER_error();
    }

    return 0;
}

