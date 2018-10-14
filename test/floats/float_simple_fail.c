//@expect fail
extern void __VERIFIER_error(void);
extern float __VERIFIER_nondet_float(void);
extern double __VERIFIER_nondet_double(void);

int main(void)
{
    float x = __VERIFIER_nondet_float();
    float y = 1.12f;

    if (x > y) {
        __VERIFIER_error();
    }

    return 0;
}
