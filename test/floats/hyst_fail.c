//@expect fail
//@flag -bound 10

extern float __VERIFIER_nondet_float(void);

#include <assert.h>
#include <stdbool.h>

typedef struct
{
    float in;
    float high;
    float low;
    bool q;
} HystBlock;

HystBlock instance;

void hyst(HystBlock* ctx)
{
    if (ctx->in > ctx->high) {
        ctx->q = true;
    } else if (ctx->in < ctx->low) {
        ctx->q = false;
    }

    assert(!(ctx->in > ctx->high) || (ctx->q == true));
    assert(!(ctx->in < ctx->low) || (ctx->q == false));
}

int main(void)
{
    instance.q = false;

    while (true) {
        instance.in = __VERIFIER_nondet_float();
        instance.high = __VERIFIER_nondet_float();
        instance.low = __VERIFIER_nondet_float();

        hyst(&instance);
    }

    return 0;
}
