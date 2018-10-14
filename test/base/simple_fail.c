//@expect fail
#include <assert.h>

int main(void)
{
    int a;
    a = 1;
    a = -1;

    assert(a != -1);
}
