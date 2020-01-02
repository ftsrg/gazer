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
/// \file An extension to the functionality provided by LLVM's Compiler.h
//
//===----------------------------------------------------------------------===//
#include <llvm/Support/Compiler.h>

// These macros may be used to disable the leak sanitizer for certain allocations.
#if LLVM_ADDRESS_SANITIZER_BUILD
    #include <sanitizer/lsan_interface.h>
    #define GAZER_LSAN_DISABLE() __lsan_disable();
    #define GAZER_LSAN_ENABLE()  __lsan_enable();
#else
    #define GAZER_LSAN_DISABLE()
    #define GAZER_LSAN_ENABLE()
#endif

