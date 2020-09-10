# About

*Gazer* is formal a verification frontend for C programs.
It provides a user-friendly end-to-end verification workflow, with support for multiple verification engines.

Currently we support two verification backends:
* `gazer-theta` leverages the power of the [theta](https://github.com/ftsrg/theta) model checking framework.
  * Currently, [v2.3.0](https://github.com/ftsrg/theta/releases/tag/v2.3.0) is tested, but newer releases might also work.
* `gazer-bmc` is gazer's built-in bounded model checking engine.

# Usage

Consider the following C program (`example.c`):
```c
#include <stdio.h>

extern int ioread32(void);

int main(void) {
    int k = ioread32();
    int i = 0;
    int j = k + 5;
    while (i < 3) {
        i = i + 1;
        j = j + 3;
    }

    k = k / (i - j);

    printf("%d\n", k);

    return 0;
}
```

The program above may attempt to divide by zero for certain values of `k`.
We can verify this program by using either verification backend:

```
$ gazer-bmc example.c
```

```
$ gazer-theta example.c
```

The verifier will return the following verification verdict:

```
Verification FAILED.
  Divison by zero in example.c at line 15 column 11.
```

## Traces

Detailed counterexample traces may be acquired by using the `-trace` option:

```
$ gazer-bmc -trace example.c
...
Verification FAILED.
  Divison by zero in example.c at line 15 column 11.
Error trace:
------------
#0 in function main():
  call ioread32() returned -11		 at 7:13
  k := -11	(0b11111111111111111111111111110101)	 at 7:13
  i := 0	(0b00000000000000000000000000000000)
  j := -11	(0b11111111111111111111111111110101)	 at 7:13
  j := ???
  i := 0	(0b00000000000000000000000000000000)
  i := 1	(0b00000000000000000000000000000001)	 at 11:15
  j := ???
  j := ???
  i := 1	(0b00000000000000000000000000000001)
  i := 2	(0b00000000000000000000000000000010)	 at 11:15
  j := ???
  j := ???
  i := 2	(0b00000000000000000000000000000010)
  i := 3	(0b00000000000000000000000000000011)	 at 11:15
  j := ???
  j := ???
  i := 3	(0b00000000000000000000000000000011)
  i := 3	(0b00000000000000000000000000000011)
  j := 3	(0b00000000000000000000000000000011)	 at 10:5
```

The trace lists each step along an errenous execution path, with the values of each program variable.

**Note:** Gazer attempts to speed up verification by optimizing and reducing the input program, and in doing so,
it may strip away some of the variables it proves to be irrelevant to the verification task.
However, such removed variables may still show up in the trace as undefined values (indicated by `???`).
This behavior can be turned off using the `-no-optimize` and `-elim-vars=off` flags.

As we can see, the errenous behavior may occur if `ioread32` returns -11.
We can request an executable test harness by using the `-test-harness=<file>` option:

```
gazer-bmc -trace -test-harness=harness.ll example.c
```

This generated test harness contains the definition of each function which was declared but not defined within the input module.
Linking this test harness against the original module yields an executable which may be used to replay the counterexample.

```
$ clang example.c harness.ll -o example_test
$ ./example_test
[1]    19948 floating point exception (core dumped)  ./example_test
```
