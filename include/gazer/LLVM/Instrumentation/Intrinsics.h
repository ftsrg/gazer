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
/// \file This file describes the various intrinsic functions that
/// Gazer uses for instrumentation.
#ifndef _GAZER_LLVM_INSTRUMENTATION_INTRINSICS_H
#define _GAZER_LLVM_INSTRUMENTATION_INTRINSICS_H

#include <llvm/IR/Instructions.h>
#include <llvm/IR/DebugInfoMetadata.h>

#include <string_view>

namespace gazer
{

class GazerIntrinsic
{
public:
    // TODO: This might break some stuff, but the verifier pass only treats intrinsics
    // having their name start with "llvm." as such.
    static constexpr char FunctionEntryPrefix[] = "llvm.gazer.function.entry";
    static constexpr char FunctionReturnVoidName[] = "llvm.gazer.function.return_void";
    static constexpr char FunctionCallReturnedName[] = "llvm.gazer.function.call_returned";
    static constexpr char FunctionReturnValuePrefix[] = "llvm.gazer.function.return_value.";
    static constexpr char InlinedGlobalWriteName[] = "llvm.gazer.inlined_global.write";

public:
    static llvm::CallInst* CreateInlinedGlobalWrite(llvm::Value* value, llvm::DIGlobalVariable* gv);
    static llvm::CallInst* CreateFunctionEntry(llvm::Module& module, llvm::DISubprogram* dsp = nullptr);

public:
    /// Returns a 'gazer.function.entry(metadata fn_name, args...)' intrinsic.
    static llvm::FunctionCallee GetOrInsertFunctionEntry(llvm::Module& module, llvm::ArrayRef<llvm::Type*> args);

    /// Returns a 'gazer.function.return_void(metadata fn_name)' intrinsic.
    static llvm::FunctionCallee GetOrInsertFunctionReturnVoid(llvm::Module& module);

    /// Returns a 'gazer.function.call_returned(metadata fn_name)' intrinsic.
    static llvm::FunctionCallee GetOrInsertFunctionCallReturned(llvm::Module& module);

    /// Returns a 'gazer.function.return_value.T(metadata fn_name, T retval)' intrinsic,
    /// where 'T' is the given return type.
    static llvm::FunctionCallee GetOrInsertFunctionReturnValue(llvm::Module& module, llvm::Type* type);

    /// Returns a 'gazer.inlined_global_write(metadata value, metadata gv_name)' intrinsic.
    static llvm::FunctionCallee GetOrInsertInlinedGlobalWrite(llvm::Module& module);
};

}

#endif
