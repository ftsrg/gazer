#include <assert.h>
// "FixPtr.*" function has hard-coded property that it returns a constant pointer without side-effects
int* FixPtr_First(void);
int* FixPtr_Second(void);
void undef_call(void);

int main2() {
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

void Enter1(void) __attribute__((used));
void Exit1(void) __attribute__((used));

int main() {
    Enter1();
    Exit1();
}
