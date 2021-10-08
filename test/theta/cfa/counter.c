// RUN: %theta -model-only -math-int "%s" -o "%t"
// RUN: cat "%t" | /usr/bin/diff -B -Z "%p/Expected/counter.theta" -
#include <assert.h>

int __VERIFIER_nondet_int();

int main(void)
{
    int sum = 0;
    int x = __VERIFIER_nondet_int();
    for (int i = 0; i < x; ++i) {
        sum += 1;
    }

    assert(sum != 0);
    return 0;
}