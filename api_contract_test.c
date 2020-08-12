#include <assert.h>
// "FixPtr.*" function has hard-coded property that it returns a constant pointer without side-effects
int* FixPtr_First(void);
int* FixPtr_Second(void);
void undef_call(void);

int main() {
    *FixPtr_First() = 5;

    // another instance is returned from another function call
    *FixPtr_Second() = 3;

    int x = *FixPtr_First();

    assert(x == 5);

    // WARNING: unknown function undef_call().
    // Side-effects might cause unsound verification.
    undef_call();
	return 0;
}
