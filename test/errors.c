// A simple basic implementation of common error functions 
// to reproduce counterexamples targeting assertion failures.
#include <stdlib.h>
#include <stdio.h>

void __assert_fail(void)
{
    fprintf(stdout, "__assert_fail executed\n");
    exit(0);
}

void __VERIFIER_error(void)
{
    fprintf(stdout, "__VERIFIER_error executed\n");
    exit(0);
}
