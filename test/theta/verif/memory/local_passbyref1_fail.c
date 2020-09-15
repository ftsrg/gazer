// REQUIRES: memory.burstall
// RUN: %theta --domain PRED_CART --refinement NWT_IT_WP "%s" | FileCheck "%s"

// CHECK: Verification FAILED
void __VERIFIER_error(void) __attribute__((__noreturn__));
void klee_make_symbolic(void* ptr, unsigned siz, const char* name);

int main(void)
{
    int x;
    klee_make_symbolic(&x, sizeof(x), "x");

    if (x == 0) {
        __VERIFIER_error();
    }

    return 0;
}
