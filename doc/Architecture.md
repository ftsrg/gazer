# Architecture

Gazer divided into several interconnected libraries.
The core library, `GazerCore` contains all components related to types and expressions. `GazerAutomaton` defines our control flow automata formalism.
This core library acts as the link between the LLVM-based verification frontend `GazerLLVM` and backends (`GazerBMC`, `GazerTheta`).
As the bounded model checker requires an SMT solver to operate,
we also provide an interface for the [Z3 SMT solver](https://github.com/Z3Prover/z3) (`GazerZ3Solver`).

The verifier component (`GazerVerifier`) offers useful interfaces for the verification backends, so that they can tie into the verification process seamlessly.
The traceability library (`GazerTrace`) is used to produce counterexample traces and is present in every step of the verification workflow.
Verification libraries are bundled together with the LLVM frontend and the traceability component into separate tools such `gazer-bmc` and `gazer-theta`.

## Verification process
The translation process starts by taking a set of C source code or LLVM bitcode files as an input.
These source files are then parsed into LLVM's control flow graph representation, the LLVM IR using the [Clang](http://clang.llvm.org/) compiler.
If multiple input files are supplied, we automatically link them together into a single LLVM IR module, using the `llvm-link` utility.
The resulting CFG is then prepared for verification by applying a set of LLVM IR transformations, including check instrumentation, built-in LLVM optimizations and custom transformations.
The instrumented and simplified program is then analyzed and translated into a control flow automaton w.r.t. a memory model.
The resulting automaton is then verified using one of our supported verification backends.

## Directory structure

### `src/`, `include/`
These folders contain the source code and associated public headers.
They are divided into a number of different sub-directories, each of them containing a library of a certain functionality.

* `ADT`: Generic containers and algorithms.
* `Automaton`: Control flow automata.
* `Core`: Types, expressions, SMT solver interfaces.
* `Trace`: Support for language-agnostic traceability.
* `LLVM`: The LLVM frontend of Gazer.
    * `Analysis`: LLVM analysis passes.
    * `Automaton`: Automata translation from the LLVM IR.
    * `Instrumentation`: Check instrumentation and other intrinsics.
    * `Memory`: Memory analysis and memory models.
    * `Trace`: Traceability and test harness generation over the LLVM IR.
    * `Transform`: LLVM transformation passes, such as program slicing and inlining.
* `Verifier`: Verification backend interfaces.
* `SolverZ3`: Support for the Z3 SMT solver.
* `Support`: Miscellaneous utilities.

### `tools/`
This directory contains runnable tools build upon the Gazer libraries.
* `gazer-bmc`: Verification tool using Gazer's built-in BMC backend.
* `gazer-theta`: Verification tool using the [theta](https://github.com/ftsrg/theta) model checking framework.
* `gazer-cfa`: CFA translation tool, used mostly for testing.

### `test/`
Functional and integration tests, using LLVM's [lit](https://llvm.org/docs/CommandGuide/lit.html) and [FileCheck](https://llvm.org/docs/CommandGuide/FileCheck.html) tools.

### `unittest/`
Our unit test suite using [Google Test](https://github.com/google/googletest).

### `scripts/`
Miscellaneous developer aide scripts, such as a test case reducer.
