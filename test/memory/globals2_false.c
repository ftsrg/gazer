//@expect fail
#include <assert.h>
#include <stddef.h>

void klee_make_symbolic(void *addr, size_t nbytes, const char *name);
int __VERIFIER_nondet_int(void);

int a, b, c;

int main(void)
{
    int x[5];
    int y[5];
    int loc = 0;

    for (int i = 0; i < 5; ++i) {
        y[i] = __VERIFIER_nondet_int();
        c = c + 1;
    }

    klee_make_symbolic(&x, sizeof(int) * 5, "x");
    klee_make_symbolic(&a, sizeof(a), "a");
    klee_make_symbolic(&loc, sizeof(loc), "loc");

    b = x[0] * y[1] * a * c + loc;

    assert(b != 0);

    return 0;
}
