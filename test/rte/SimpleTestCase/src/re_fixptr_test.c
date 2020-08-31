#include <assert.h>

int* fixPtr_five(void);
int* fixPtr_three(void);

void re_fixptr_test() {
    *fixPtr_five() = 5;
    *fixPtr_three() = 3;
    assert(*fixPtr_five() == 5);
    assert(*fixPtr_three() == 3);
}
