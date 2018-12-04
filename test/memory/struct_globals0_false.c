//@expect fail
#include <assert.h>
#include <stddef.h>

struct MyData {
    int a;
    int b;
};

void klee_make_symbolic(void *addr, size_t nbytes, const char *name);
int __VERIFIER_nondet_int(void);

int a, b, c;
struct MyData M;

int main(void)
{
    int x[5];
    int y[5];
    int loc = 0;
    struct MyData S[3];

    for (int i = 0; i < 5; ++i) {
        y[i] = __VERIFIER_nondet_int();
        c = c + 1;
    }

    M.a = 1;

    klee_make_symbolic(&S, sizeof(S), "S");
    klee_make_symbolic(&x, sizeof(int) * 5, "x");
    klee_make_symbolic(&a, sizeof(a), "a");
    klee_make_symbolic(&loc, sizeof(loc), "loc");

    b = x[0] * y[1] * a * c + loc - S[1].b  + S[2].a + M.a;

    assert(b != 0);

    return 0;
}
