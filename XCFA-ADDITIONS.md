# Additions in XCFA branch

`TODO merge with README.md and developer docs after stabilizing branch`

`TODO add (back?) theta to lit tool for integration tests.`

**Note:** All new files modify `CMakeLists.txt`, that is not listed unless for a non-trivial modification.

## `TODO`: remove unnecessary modifications

Some are annotated in Github.

- `test/theta/cfa/Expected/counter.theta`

## Disable loops to recursion transformation

### What is it?

Loop handling is important for only a small portion of code (specifically for Gazer's own BMC algorithm).
This is disabled.

Loops (as a Gazer CFA) might have more than one output, which makes it incompatible with XCFA.

### List of changed files

- `FunctionToCfa.h` and `ModuleToAutomata.cpp`: infos are not passed around for loops;
	These do *not* contain loop registration (as far as I've seen), only that must be disabled.
	Uncomment is needed for BMC to work. 

### Further changes needed for merging with master

`TODO add flag to disable loop transformations, force-disable loops when generating XCFA, force-enable for BMC. Probably CFA does not need this.`

## XCFA generation

### What is it?

Generate XCFA instead of CFA. Globals are supported (with `Global variables support`)

Tracing is disabled.

Also some steps were taken to improve the code related to code generation.

### List of changed files

- `tools/gazer-theta` directory: no CFA can be generated as of now. Also contains some code improvements.

### Further changes needed for merging with master

`TODO create common code for CFA/XCFA generation, support both CFA and XCFA (CFA generation is missing)`

`TODO error locations are supported on non-main functions, so global inlining pass is not needed`

## Global variables support

### What is it?

Also added a GlobalVariableExtensionPoint to enable memory models declare global variables.

### List of changed files

- `ModuleToAutomata.cpp`: `declareGlobalVariables` is called here before the first function for the very
first memory instruction handler instance.

- The `AutomataSystem` class in `Cfa.h` is patched to enable adding variables not attached to any CFA (hence, global).
  Also added a way to iterate through them.

### Further changes needed for merging with master

## Simple memory model for globals and alloca'd locals

### What is it?

The simple memory models is a simple memory model asserting that there are no pointer-int casts, global arrays and
various pointer magic.

There are broken assumptions made in Gazer, as there is no 'global' state assumed, everything is considered local.
Gazer's solution is to pass a memory object, as it is easier to reason about. With global variables handled in Theta,
this is not needed.

Memory models are handled by-function. This clashes with the original global variable support.

### List of changed files

- `src/LLVM/Memory/SimpleMemoryModel.cpp`: Implementation.

- `src/LLVM/Memory/MemoryModel.cpp`: removed havoc memory model, simple memory model is used instead.
  Rework needed.

### Further changes needed for merging with master

`TODO memory models are handled by-function. This clashes with global variable support`

`TODO remove hack that changes "havoc" implementation to simple memory model`

## Swap function call with an inline LLVM implementation; decoupled analysis and inline implementation

### What is it?

Theta has some limitations (CFA or XCFA, whatever), and that limits our scope. Also, there are special knowledge around
some functions: e.g. malloc(n) returns a `dereferencable(n) noalias` pointer, but it's not `speculatable`.
See the LLVM reference for the meaning of these function attributes).

Some special functions can be transformed in such a way that there is no pointer (magic) involved.

There are two passes: one analyzing which functions have an implementation applicable to this (set of) transformation(s).

Functions with `fixptr` (non-LLVM) annotation are transformed as such:

```C
T* FixPtr(void)
T* variable = FixPtr()

---

T FixPtr.fixptr = undef
T* variable = gazer.FixPtr.fixptr
```

Say, the hidden implementation is something trivial like `FixPtr(void) { return FixPtr_global; }`.

**Note:** Currently there only one function handled; specific to a demo, and it does not have a similar STL function. 

Unknown functions with special signatures (returning or passing a parameter) must be such that  

### List of changed files

- `src/LLVM/Instrumentation/AnnotateSpecialFunctions.cpp`: Adds in-LLVM annotations from some sources.

- `src/LLVM/Instrumentation/SimplifySpecialFunctions.cpp`: Transforms the calls. Also this requires the
  annotation pass.

- `src/LLVM/LLVMFrontend.cpp`: Registers the pass to the pipeline.


### Further changes needed for merging with master

`TODO rename passes, as the name clashes with SpecialFunctions interface of Gazer`

`TODO find a good way to interface with AnnotateSpecialFunctions analysis pass`

## Demo for non-reentrant mutex usage checks

### What is it?

A simple check, unused, may be deleted.

Calls with the pattern `Enter*` and `Exit*` are transformed to have a check, when transformed to CFA. 

### List of changed files

- `DefaultChecks.cpp` and some other.

### Further changes needed for merging with master

`TODO remove or move to separate file`

## Support for handling functions without implementation in LLVM

### What is it?

Unimplemented functions are handled in a different way than in the original implementation: they were unknown
functions, which might return a value, and, as the implementations are unknown, they are converted havocing variable.

Now this usage is broken, as they are now used, instead, as a way to include functions unknown to Gazer, but known to
Theta.

Intrinsics are handled the same way as the original implementation. 

### List of changed files

- `Callgraph.h` is patched to enable Calls with no nodes attached to the target.

- The `AutomataSystem` class in `Cfa.h` is patched: createCfa might create a `Cfa` that is not registered to the
  `AutomataSystem`. This is done to ensure iterating through CFAs one can be sure there is an implementation.

- `FunctionToCfa.h` contains data attached to procedures. When a procedure is missing, there was an error. Added
a way to ask whether a function has related (generation) info.  

- `ModuleToAutomata.cpp`: `blocksToCfa` is created only for declared functions.
  Unsure if this change is needed for the current implementation (as the generation context's procedures might not
  actually contain the given procedures)
	
- `BlocksToCfa::handleCall` in `ModuleToAutomata.cpp` is reordered,
  `!callee->isDeclaration()` branch handles the new implementation. Original handling must `return true`,
  nothing else is needed. (Like for gazer.*) No unrelated changes were made in this function.

### Further changes needed for merging with master

`TODO rename hasInfoFor(function)` to `isImplemented(function)`?

`TODO add comment: only CFAs with implementation are registered; And to Cfa: One might not have an implementation`    

`TODO Gazer must know which functions are implemented at a later stage, as use-cases clash`

`TODO generation context's procedures need not contain unimplemented procedures`

## Passing stub XCFA files 

### What is it?

Strongly coupled `Support for handling functions without implementation in LLVM`.

The unimplemented functions can be defined in XCFA files in Theta. Not wired to a setting :(

Merging is done through a recent addition to Theta XCFA.

### List of changed files

- `tools/gazer-theta/lib/ThetaVerifier.cpp`: Passing the list of XCFAs (instead of the single XCFA) to Theta.
- `tools/gazer-theta/lib/ThetaVerifier.h`: Adding list of XCFA files to the definition of configuration passed
  around for Theta verification. 

### Further changes needed for merging with master

`TODO wire this to command-line interface to gazer-theta`

## New driver: gazer-theta-re

### What is it?

This is a demo-specific addition, but some parts can be reused, I think.

Iterates through a list of `.c` files, and runs a modified workflow to it.

The most important: it tells `AnnotateSpecialFunctions` pass which functions (unknown to Gazer) have special
implementations. Anyway, the way it does this is plain ugly (maybe Linux-specific?), so that one must be fixed.

### List of changed files

- `test/lit.cfg`: The directory structure for testing contains a `run.config` file, that must be parsed by `lit`.
  Also the binary is linked so the configuration can use it.

- `tools/gazer-theta/gazer-theta-re.cpp`: implementation

- `test/rte`: test cases related to the new driver.

- `tools/gazer-theta/lib/ThetaVerifier.cpp`: Multiple XCFA can be added.	

### Further changes needed for merging with master

`TODO Merge common code with gazer-theta.cpp`