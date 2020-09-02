# About

*Gazer* is formal a verification frontend for C programs.
It provides a user-friendly end-to-end verification workflow, with support for multiple verification engines.

Currently we support two verification backends:
* `gazer-theta` leverages the power of the [theta](https://github.com/ftsrg/theta) model checking framework.
  * Currently, [v1.3.0](https://github.com/ftsrg/theta/releases/tag/v1.3.0) is tested, but newer releases might also work.
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

## Development on Windows

One possibility, devised here, is to install Docker, and use it as a Linux subsystem.

After installing Docker, one must create an image, that is capable of running the tests.

- Install docker
- Download gazer project and theta project and LLVM source code (later referred to as `$HOST_THETA_DIR` and `$HOST_GAZER_DIR`, `$HOST_LLVM_DIR`).
- Modify gazer's dockerfile to include `lit` (used in regression tests) and it's necessary components: `psutil` (for handling case-by-case timeouts)
- Modify gazer's dockerfile to use theta project's source code instead of downloading the source.
- Mount Theta and Gazer, LLVM inside Docker. Publish port 8000:

  ```sh
  docker run -i -t -p 127.0.0.1:8000:8000 --name dev-container \
      --mount type=bind,source="$HOST_THETA_DIR/theta",target=/host/theta \
      --mount type=bind,source="$HOST_LLVM_DIR/theta",target=/host/llvm \
      --mount type=bind,source="$HOST_GAZER_DIR\gazer",target=/host/gazer \
      theta-gazer-dev bash`
  ```
      
  ```dev.dockerfile
  FROM ubuntu:18.04

  RUN apt-get update && \
      apt-get install -y build-essential git cmake \
      wget sudo vim lsb-release \
      software-properties-common zlib1g-dev \
      openjdk-11-jdk

  # fetch LLVM and other dependencies
  RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add - && \
      add-apt-repository "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-9 main" && \
      apt-get update && \
      add-apt-repository ppa:mhier/libboost-latest && \
      apt-get update && \
      apt-get install -y clang-9 llvm-9-dev llvm-9-tools llvm-9-runtime libboost1.70-dev && \
      ln -s `which clang-9` /usr/bin/clang && \
      ln -s `which llvm-link-9` /usr/bin/llvm-link && \
      ln -s /usr/lib/llvm-9/build/utils/lit/lit.py /usr/bin/lit && \
      ln -s /usr/lib/llvm-9/bin/FileCheck /usr/bin/FileCheck && \

  # create a new user `user` with the password `user` and sudo rights
  RUN useradd -m user && \
      echo user:user | chpasswd && \
      cp /etc/sudoers /etc/sudoers.bak && \
      echo 'user  ALL=(root) NOPASSWD: ALL' >> /etc/sudoers

  USER user

  ENV GAZER_DIR /host/gazer
  ENV THETA_DIR /host/theta

  COPY ./build.sh /host/build.sh
  COPY ./first-run.sh /host/first-run.sh

  WORKDIR $GAZER_DIR
  CMD /bin/bash
  ```

- When first running, run LLVM's CMakefile with the proper configuration, and generate Makefiles. This (binding LLVM) is used to be able to import LLVM as a project and use its' sources (and comments) for development.
- When first running, it's a good idea to create scripts capable of handling basic usage:

  Useful commands/scripts for use inside Docker
  ```sh
  # build Theta (xcfa only)
  cd /host/theta
  ./gradlew :theta-xcfa-cli:shadowJar
  ```

  ```sh
  # bulid gazer (runnable-entity checker only)
  cd /host/gazer/build
  make gazer-theta-re
  ```

  ```sh
  # run gazer's regression tests with necessary options
  GAZER_TOOLS_DIR=/host/gazer/build/tools
  make check-functional
  ```
  
  ```sh
  # run gazer with necessary options.
  /host/gazer/build/tools/gazer-theta/gazer-theta-re --theta-path=/host/theta/subprojects/xcfa-cli/build/libs/theta-xcfa-cli-0.0.1-SNAPSHOT-all.jar --lib-path=/host/theta/lib/ "$@"
  ```
  
  (TBD)
