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
#include "gazer/LLVM/Automaton/SpecialFunctions.h"

using namespace gazer;

const SpecialFunctions SpecialFunctions::EmptyInstance;

auto SpecialFunctions::get() -> std::unique_ptr<SpecialFunctions>
{
    auto result = std::make_unique<SpecialFunctions>();

    // Verifier assumptions
    result->registerHandler("verifier.assume",   &SpecialFunctions::handleAssume, SpecialFunctionHandler::Memory_Pure);
    result->registerHandler("llvm.assume",       &SpecialFunctions::handleAssume, SpecialFunctionHandler::Memory_Pure);

    return result;
}

void SpecialFunctions::registerHandler(
    llvm::StringRef name,
    SpecialFunctionHandler::HandlerFuncTy function,
    SpecialFunctionHandler::MemoryBehavior memory)
{
    auto result = mHandlers.try_emplace(name, function, memory);
    assert(result.second && "Attempt to register duplicate handler!");
}

auto SpecialFunctions::handle(llvm::ImmutableCallSite cs, llvm2cfa::GenerationStepExtensionPoint& ep) const
    -> bool
{
    assert(cs.getCalledFunction() != nullptr);

    // Check if we have an appropriate handler
    auto it = mHandlers.find(cs.getCalledFunction()->getName());
    if (it == mHandlers.end()) {
        return false;
    }

    it->second(cs, ep);
    return true;
}

// Default handler implementations
//===----------------------------------------------------------------------===//

void SpecialFunctions::handleAssume(llvm::ImmutableCallSite cs, llvm2cfa::GenerationStepExtensionPoint& ep)
{
    const llvm::Value* arg = cs.getArgOperand(0);
    ExprPtr assumeExpr = ep.getAsOperand(arg);

    ep.splitCurrentTransition(assumeExpr);
}
