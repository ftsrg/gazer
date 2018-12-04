//@expect fail
#include <assert.h>
#include <stddef.h>

void klee_make_symbolic(void *addr, size_t nbytes, const char *name);
int __VERIFIER_nondet_int(void);

int a, b, c;

int main(void)
{
    klee_make_symbolic(&a, sizeof(a), "a");
    klee_make_symbolic(&c, sizeof(c), "c");
    for (int i = 0; i < 5; ++i) {
        b = c * 2 + a;
    }

    assert(c != 0);

    return 0;
}
