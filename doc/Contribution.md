# Contribution Guidelines

## Coding style and naming convention

You can use `clang-format` with our provided style configuration file to reformat source code files to the appropriate coding style before commiting a patch.
Some important points to keep in mind:

* Use 4 spaces for indentation.
* Namespaces do not create a new and indentation level.
* Use K&R style brace placement: newline before the brace following a declaration (type or function), no newline in the case of control structures.
* The soft line limit is **80** characters, but we allow lines to be up to **120** characters long.

### Naming conventions

* Use `PascalCase` for types and global variables.
* Use `camelCase` for (member) functions and local variables.
* Prefix private or protected class members with the letter `m`, e.g. `mBackend`, `mAnalysis`, etc.

For justified cases, it is allowed to use a naming convention separate from the one described above.
Some of the currently existing exceptions are:

* **Expression builders:** we use the class name for the different creating methods (`builder->Add(...)` instead of `builder->createAdd(...)`) as they are more readable when constructing nested expressions.
* **Expression matchers:** we use a `m_CLASSNAME` naming convention for matchers (`m_Add`, `m_And`, etc.), as this lines up with the naming convention of the similar instruction matching utility in LLVM.
* **Uses of the pImpl idiom:** when using the _pImpl_ idiom, implementation pointers are named `pImpl` instead of `mImpl`.

### File organization

Please start each source file with the following license header, possibly followed by
the documentation of the source file:
```cpp
//==-------------------------------------------------------------*- C++ -*--==//
//
// Copyright 2019 Contributors to the Gazer project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//===----------------------------------------------------------------------===//
//
/// \file The functions defined here have some particularly useful features.
//
//===----------------------------------------------------------------------===//
```

## Testing guide
Gazer comes with a unit and functional test suite.
The former aims to verify small building blocks of the system and is built upon the [Google Test](https://github.com/google/googletest) unit testing framework.
The functional test suite is used to verify the correct behavior of the whole verification workflow.

### Functional tests
The functional test suite is built upon LLVM's integrated tester, [lit](https://llvm.org/docs/CommandGuide/lit.html).
Each test case in `lit` is a single source code file with some `RUN` directives indicating commands to execute.
The test run is deemed successful if _all_ `RUN` commands return successfully.
Test assertions may be placed by using UNIX utilities like `grep` or `diff` and LLVM's powerful testing tool [FileCheck](https://llvm.org/docs/CommandGuide/FileCheck.html) to embed checks of the command output into the test case file.

```c
// RUN: %bmc -memory=havoc "%s" | FileCheck "%s"
// RUN: %bmc -memory=flat "%s" | FileCheck "%s"

// CHECK: Verification FAILED
#include <assert.h>

int main(void) {
    assert(0);
}
```

### Test case reduction
Testing verification tools is challenging, mostly due to their complexity and possibly large running time.
This means catching the cause of a false negative or false positive result in a possibly large source file can be difficult and time-consuming.

In order to ease this process, we rely heavily on the delta debugging tool [C-Reduce](https://embed.cs.utah.edu/creduce/).
The tool attempts to build a minimal working example for a behavior from a source code file that causes the tool to exhibit said behavior.
C-Reduce identifies such _interesting_ files using a _interestingness test_.

The `testreduce.py` script allows developers to generate interestingness tests for Gazer tools easily.
It currently supports debugging for crashes (through its `pattern` target)  unreproducible counterexamples (`false-cex`), false positives (`invalid-fail`), and false negatives (`invalid-success`).
False positives and negatives must be compared to a _safe tool_, which could eithber be one of Gazer's supported tools (or even the same tool with a different conifguration), [CPAChecker](http://cpachecker.sosy-lab.org/) or [CBMC](https://www.cprover.org/cbmc/).

For example, if you wish to generate an interestingness test for a false negative result (compared agains CPAChecker), you can use the following command.
Generally it is advisable to add the `-Werror` flag to the tool args to somewhat prevent building test cases that exhibit undefined behavior.

```bash
scripts/testreduce.py invalid-success --tool bmc --tool-args="-Werror" --safe-tool cpa --safe-tool-args="-default -spec /path/to/spec.spc" program.c > check.sh
```

This command creates a working copy of `program.c` and writes the appropriate interestingness test script to the standard output.
Then you can invoke `creduce` on the generated interestingness test and the working copy:

```bash
creduce check.sh program.c_creduce.c
```
