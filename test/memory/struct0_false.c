//@expect fail
#include <stddef.h>

void klee_make_symbolic(void *addr, size_t nbytes, const char *name);
int __VERIFIER_nondet_int(void);
void __VERIFIER_error(void) __attribute__((noreturn));

void update(int* val);

struct MyData
{
    int a;
    int b;
};

int main(void)
{
    struct MyData S;

    klee_make_symbolic(&S, sizeof(S), "S");

    S.a = S.a + S.b;

    if (S.a != 0) {
        __VERIFIER_error();
    }

    return 0;
}
