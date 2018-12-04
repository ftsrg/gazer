//@expect fail
#include <assert.h>
#include <stddef.h>

void klee_make_symbolic(void *addr, size_t nbytes, const char *name);
int __VERIFIER_nondet_int(void);

int main(void)
{
    int x[5];
    int y[5];

    for (int i = 0; i < 5; ++i) {
        y[i] = __VERIFIER_nondet_int();
    }

    klee_make_symbolic(&x, sizeof(int) * 5, "x");

    assert(x[0] != y[0]);

    return 0;
}
