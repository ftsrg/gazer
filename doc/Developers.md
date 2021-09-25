# For Developers

## Building

1. Download and install CMake (3.6 is the minimum required).
2. Download and install LLVM 9. Make sure you use the exact major version.

   **Note**: When building LLVM by hand, remove tests for LLVM with `cmake -DLLVM_INCLUDE_TESTS=OFF ...`. See [this](https://github.com/pytorch/glow/issues/1675) for more information.

3. Install the Boost library (version 1.70 or later).
4. Create a build directory.
    ```
    mkdir build
    cd build
    ```
5. Run cmake and the build system of your choice in this build directory.
    ```
    cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE="Debug" -DLLVM_DIR=<directory/of/LLVMConfig.cmake> ..
    make
    ```
   In addition to the standard cmake flags, Gazer builds may be configured with the following Gazer-specific flags:
   * **GAZER_ENABLE_UNIT_TESTS:** Build Gazer unit tests. Defaults to ON.
   * **GAZER_ENABLE_SANITIZER:** Enable the address and undefined behavior sanitizers. Defaults to OFF.
   * **GAZER_SHARED_LLVM:** Link LLVM components dynamically (e.g. using `libLLVM.so`). Only recommended for debug builds, defaults to OFF.

## Test

Gazer's unit tests may be run by invoking:
```
make check-unit
```

To run the functional test suite, LLVM's `lit` tool and `psutil` must be installed with:
```
pip install lit psutil
```

After installing `lit`, you can run the functional test suite using:
```
make check-functional
```
