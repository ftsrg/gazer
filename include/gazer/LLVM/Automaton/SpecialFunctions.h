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
#ifndef GAZER_LLVM_AUTOMATON_SPECIALFUNCTIONS_H
#define GAZER_LLVM_AUTOMATON_SPECIALFUNCTIONS_H

#include "gazer/LLVM/Automaton/ModuleToAutomata.h"

#include <functional>

namespace gazer
{

/// This class is used to handle calls to certain known special and intrinsic functions.
class SpecialFunctionHandler
{
public:
    using HandlerFuncTy = std::function<void(const llvm::CallBase*, llvm2cfa::GenerationStepExtensionPoint& ep)>;

    enum MemoryBehavior
    {
        Memory_Pure,           ///< This function does not modify memory.
        Memory_Default,        ///< Handle memory definitions according to the rules of the used memory model.
        Memory_MayDefinePtr,   ///< Define memory object reachable through its pointer operand(s).
        Memory_DefinesAll      ///< Clobber and define all known memory objects.
    };

    explicit SpecialFunctionHandler(HandlerFuncTy function, MemoryBehavior memory = Memory_Default)
        : mHandlerFunction(std::move(function)), mMemory(memory)
    {}

    void operator()(const llvm::CallBase* cs, llvm2cfa::GenerationStepExtensionPoint& ep) const
    {
        return mHandlerFunction(cs, ep);
    }

private:
    HandlerFuncTy mHandlerFunction;
    MemoryBehavior mMemory;
};

class SpecialFunctions
{
    static const SpecialFunctions EmptyInstance;
public:
    /// Returns a SpecialFunctions object with the default handlers
    static std::unique_ptr<SpecialFunctions> get();

    /// Returns a handler without any registered handlers
    static const SpecialFunctions& empty() { return EmptyInstance; }

    void registerHandler(
        llvm::StringRef name, const SpecialFunctionHandler::HandlerFuncTy& function,
        SpecialFunctionHandler::MemoryBehavior memory = SpecialFunctionHandler::Memory_Default);

    template<class Range>
    void registerMultipleHandlers(
        Range& names, const SpecialFunctionHandler::HandlerFuncTy& function,
        SpecialFunctionHandler::MemoryBehavior memory = SpecialFunctionHandler::Memory_Default)
    {
        for (const auto& name : names) {
            this->registerHandler(name, function, memory);
        }
    }

    bool handle(const llvm::CallBase* cs, llvm2cfa::GenerationStepExtensionPoint& ep) const;

    // Some handlers for common cases
    static void handleAssume(const llvm::CallBase* cs, llvm2cfa::GenerationStepExtensionPoint& ep);

private:
    llvm::StringMap<SpecialFunctionHandler> mHandlers;

};

} // namespace gazer

#endif