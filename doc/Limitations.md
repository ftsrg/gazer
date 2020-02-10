# Limitations

This page lists some of the known and currently untackled limitations of Gazer.

## False negatives and false positives

### Assumptions made by Clang

The verification process starts by translating the input program into LLVM IR using Clang,
therefore it inherits some of the assumptions Clang makes during compilation.
This is mostly noticable in the presence of undefined and implementation-defined behavior.
As an example, when compiled with Clang 9.0.1 the following program behaves correctly as the value of `y` is `3`.
However, with GCC 9.2.0 the result is actually `4`, making the assertion fail.

```c
#include <assert.h>
int main() {
  int x = 0;
  int y = (x = 1) + (x = 2);

  assert(y == 3);
    
  return 0;
}
```

The resulting LLVM IR has no knowledge whether the input program was well-defined, and explicitly
sets the value of `y` to be `3`:

```
  %2 = alloca i32, align 4              ; x
  %3 = alloca i32, align 4              ; y
  store i32 0, i32* %2, align 4         ; x = 0
  store i32 1, i32* %2, align 4         ; x = 1
  store i32 2, i32* %2, align 4         ; x = 2
  store i32 3, i32* %3, align 4         ; y = 3
  %6 = load i32, i32* %3
  %7 = icmp eq i32 %6, 3                ; y == 3
  br i1 %7, label %8, label %error
```

By looking at the LLVM IR module above, Gazer will (erroneously) conclude that the program is correct,
even though ideally it _should_ report that an assertion failure may be reachable due to undefined behavior.

### Undefined behavior in LLVM

LLVM has multiple constructs to represent undefined behavior, out of which we currently rely on `undef` the most.
The `undef` values are special constants, available for each LLVM type.
They have interesting semantics: the compiler is allowed to choose any bit-pattern for such a value, enabling optimizations which would not be legal otherwise.
However, each instance of an `undef` is independent from the others, therefore it is possible that a different value is chosen for each instance.

Consider the following C program. Variable `b` is not initialized, therefore the first branch condition is undefined.
Furthermore, if the first branch condition was false, then the second branch condition is undefined as well.

```c
int a;
void __VERIFIER_error();
int main() {
  int b;
  if (b)
    b = a;
  if (b)
    __VERIFIER_error();
}
```

If we can assume that the value of an uninitialized variable stays the same, the program above cannot fail:
if the value of `b` is chosen to be zero, then both branches are false.
If `b` is originally non-zero, the assignment will change its value to zero (global variables are implicitly initialized to `0`),
therefore the second branch will always be false.

LLVM will not make the deduction above, therefore the resulting module will have two `undef` constants, one for each undefined branch:

```
define dso_local i32 @main() #0 {
  %1 = icmp ne i32 undef, 0
  br i1 %1, label %2, label %4

2:
  %3 = load i32, i32* @a, align 4
  br label %4

4:
  %.0 = phi i32 [ %3, %2 ], [ undef, %0 ]
  %5 = icmp ne i32 %.0, 0
  br i1 %5, label %6, label %7

6:
  call void (...) @__VERIFIER_error()
  br label %7

7:
  ret i32 0
}
```

At this point Gazer will have no knowledge how these `undef` values came to be and will correctly assume that their values may not be the same.
This will make the verification fail - a correct verdict due to the presence of undefined behavior, but the resulting counterexample may be unreproducible.

