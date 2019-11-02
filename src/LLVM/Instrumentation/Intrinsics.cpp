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
#include "gazer/LLVM/Instrumentation/Intrinsics.h"

#include <llvm/IR/Module.h>

using namespace gazer;

llvm::FunctionCallee GazerIntrinsic::GetOrInsertFunctionEntry(llvm::Module& module, llvm::ArrayRef<llvm::Type*> args)
{
    std::vector<llvm::Type*> funArgs;
    funArgs.push_back(llvm::Type::getMetadataTy(module.getContext()));
    funArgs.insert(funArgs.end(), args.begin(), args.end());

    auto funTy = llvm::FunctionType::get(llvm::Type::getVoidTy(module.getContext()), funArgs, false);

    std::string buffer;
    llvm::raw_string_ostream rso{buffer};

    rso << FunctionEntryPrefix;
    for (auto& arg : args) {
        rso << '.';
        arg->print(rso, false, true);
    }
    rso.flush();

    return module.getOrInsertFunction(
        rso.str(),
        funTy
    );
}

llvm::FunctionCallee GazerIntrinsic::GetOrInsertFunctionReturnVoid(llvm::Module& module)
{
    return module.getOrInsertFunction(
        FunctionReturnVoidName,
        llvm::Type::getVoidTy(module.getContext()),
        llvm::Type::getMetadataTy(module.getContext())
    );
}

llvm::FunctionCallee GazerIntrinsic::GetOrInsertFunctionCallReturned(llvm::Module& module)
{
    return module.getOrInsertFunction(
        FunctionCallReturnedName,
        llvm::Type::getVoidTy(module.getContext()),
        llvm::Type::getMetadataTy(module.getContext())
    );
}

llvm::FunctionCallee GazerIntrinsic::GetOrInsertFunctionReturnValue(llvm::Module& module, llvm::Type* type)
{
    assert(type != nullptr);

    std::string nameBuffer;
    llvm::raw_string_ostream rso(nameBuffer);
    type->print(rso, false, true);
    rso.flush();

    // Insert a new function for this mark type
    return module.getOrInsertFunction(
        FunctionReturnValuePrefix + rso.str(),
        llvm::Type::getVoidTy(module.getContext()),
        llvm::Type::getMetadataTy(module.getContext()),
        type
    );
}

llvm::FunctionCallee GazerIntrinsic::GetOrInsertInlinedGlobalWrite(llvm::Module& module)
{
    return module.getOrInsertFunction(
        InlinedGlobalWriteName,
        llvm::Type::getVoidTy(module.getContext()),
        llvm::Type::getMetadataTy(module.getContext()),
        llvm::Type::getMetadataTy(module.getContext())
    );
}
